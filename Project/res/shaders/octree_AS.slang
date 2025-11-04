#include "ray.slang"
#include "default_defines.slang"

struct PushConstants {
  float3 camera_position;
  float3 camera_front;
  float3 camera_right;
  float3 camera_up;
  float4x4 octree_world;
  float4x4 octree_world_inverse;
  float4x4 octree_scale_inverse;
};

struct OctreeNode {
  uint32_t data;

  __init(uint32_t data) {
    this.data = data;
  }

  // Flags
  property bool isSolid {
    get { return ((data >> 30) & 0x1) != 0; }
  }

  // Node properties
  property uint32_t childMask { // Only lower 8 bits used
    get { return (data >> 22) & 0xFF; }
  }
  property uint32_t offset {
    get { return (data & 0x003FFFFF); }
  }

  // Lead properties
  property float r {
    get { return ((data & 0x00FF0000) >> 16) / 255.; }
  }
  property float g {
    get { return ((data & 0x0000FF00) >> 8) / 255.; }
  }
  property float b {
    get { return ((data & 0x000000FF) >> 0) / 255.; }
  }

  property float3 colour {
    get { return float3(this.r, this.g, this.b); }
  }
};

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

[[vk::binding(0, 1)]]
StructuredBuffer<OctreeNode> i_Octree;

struct StackMember {
  uint parent;
  float3 minBound;
  float tMax;
}

func scaleExpToFloat(uint scale_exp) -> float {
    return asfloat((127 << 23) + (1 << scale_exp)) - 1.;
}

func floorScale(float3 value, uint scale_exp) -> float3 {
  return asfloat(asuint(value) & ~((1 << scale_exp) - 1));
}

func calculateChildIndex(float3 position, int3 step_dir, uint scale_exp) -> uint {
  const float3 cell_min = floorScale(position, scale_exp + 1);

  const float scale = scaleExpToFloat(scale_exp);

  const float3 center = cell_min + scale;

  const bool3 mask = position > center || (position == center && step_dir > 0);
  const int3 octant_mask = int3(1, 4, 2);

  // Dot product requires shaderIntegerDotProduct
  return mask.x * octant_mask.x + mask.y * octant_mask.y + mask.z * octant_mask.z;
}

func traverse(in ray_original : Ray) -> HitRecord {
  const float3 min_bound = float3(1);
  const float3 max_bound = float3(2);
  const float3 dimensions = max_bound - min_bound;

  const float4 origin_w = mul(float4(ray_original.origin, 1), push_constants.octree_world_inverse);
  const float4 direction_w = mul(float4(ray_original.direction, 1), push_constants.octree_scale_inverse);

  Ray ray = Ray(
    origin_w.xyz / origin_w.w,
    normalize(direction_w.xyz / direction_w.w)
  );

  HitRecord hit;
  float boundingTMin, boundingTMax;
  if (!ray.aabb(min_bound, max_bound, boundingTMin, boundingTMax, EPS, MAX_FLOAT)) {
    return hit;
  }

  uint stack[MAX_DEPTH];

  uint scale_exp = 22;

  uint node_index = 0;
  OctreeNode node = i_Octree[node_index];

  float3 position = clamp(ray.calculate(boundingTMin), float3(1), float3(2 - EPS));

  float3 normal = calculateNormal(position, min_bound, max_bound);

  const int3 step_dir = sign(ray.direction);

  for (int i = 0; i < STEP_LIMIT; i++) {

#ifdef HEATMAP
    hit.intersection_checks++;
#endif

    // Descend
    uint child_index = calculateChildIndex(position, step_dir, scale_exp);
    while(!node.isSolid && ((node.childMask >> child_index) & 1) != 0) {
      stack[scale_exp] = node_index;

#ifdef HEATMAP
      hit.intersection_checks++;
#endif

      node_index += node.offset + countbits(node.childMask >> (child_index + 1));
      node = i_Octree[node_index];

      scale_exp--;
      child_index = calculateChildIndex(position, step_dir, scale_exp);
    }

    if (node.isSolid) { // Is Leaf node
      hit.hit = true;
      hit.hit_position = position;
      hit.colour = node.colour;
      hit.normal = normal;

      return hit;
    }

    const float scale = scaleExpToFloat(scale_exp);

    float3 cell_min = floorScale(position, scale_exp);
    float3 side_dist = (cell_min + (max(step_dir, int3(0)) * scale) - ray.origin) * ray.inverse_direction;
    float tmax = min(min(side_dist.x, side_dist.y), side_dist.z);
    normal = -step_dir * (tmax == side_dist);

    const float3 neighbour_min = select(tmax == side_dist, cell_min + scale * step_dir, cell_min + EPS);
    const float3 neighbour_max = neighbour_min + float3(scale) - select(tmax == side_dist, EPS, 0.);
    
    float3 previous_position = position;
    position = clamp(ray.calculate(tmax), neighbour_min, neighbour_max);

    // Ascend if the parent bounds now differ
    float3 parent_cell_min = floorScale(previous_position, scale_exp + 1);
    float3 new_cell_min = floorScale(position, scale_exp + 1);
    bool3 diff = abs(parent_cell_min - new_cell_min) > EPS;
    while (diff.x || diff.y || diff.z) {

#ifdef HEATMAP
      hit.intersection_checks++;
#endif

      scale_exp++;
       
      if (scale_exp >= MAX_DEPTH) {
        return hit;
      }

      node_index = stack[scale_exp];
      node = i_Octree[node_index];

      parent_cell_min = floorScale(previous_position, scale_exp + 1);
      new_cell_min = floorScale(position, scale_exp + 1);
      diff = abs(parent_cell_min - new_cell_min) > EPS;
    }
  }

  return hit;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
#ifdef CYCLES
  uint64_t start_cycles = clockARB();
#endif
  int2 pixelCoord = int2(dispatchThreadID.xy);

  int width, height;
  o_Image.GetDimensions(width, height);
  float aspect = float(width) / float(height);

  float2 uv = pixelCoord / float2(width, height);

  Ray ray = Ray(aspect,
                uv,
                push_constants.camera_position,
                push_constants.camera_front,
                push_constants.camera_right,
                push_constants.camera_up
  );

  HitRecord hit = traverse(ray);

#ifdef CYCLES
uint64_t end_cycles = clockARB();
#endif

#ifdef HEATMAP
  if (hit.intersection_checks != 0) {
      float3 mix = lerp(MIN_INTERSECTION_COLOUR, MAX_INTERSECTION_COLOUR, float3(hit.intersection_checks / float(INTERSECTION_MAX)));
    o_Image[pixelCoord] = float4(mix, 1.);
  } else {
    o_Image[pixelCoord] = float4(0);
  }
#elif defined(CYCLES)
  float3 mix = lerp(MIN_CYCLE_COLOUR, MAX_CYCLE_COLOUR, float3((end_cycles - start_cycles) / float(CYCLE_MAX)));
  o_Image[pixelCoord] = float4(mix, 1.);
#else
  if (hit.hit) {
    // const float3 light_position = float3(5, -2, 5);
    // const float3 light_colour = float3(1.0);
    // const float3 object_colour = float3(1);
    // // const float3 object_colour = hit.colour;
    //
    // const float ambient_strength = 0.3;
    //
    // const float3 ambient = light_colour * ambient_strength;
    //
    // const float3 light_dir = normalize(light_position - hit.hit_position);
    // const float diffuse_strength = max(dot(light_dir, hit.normal), 0);
    //
    // // const float falloff = 1 / pow(length(light_position - hit.hit_position), 2);
    //
    // const float3 diffuse = diffuse_strength * light_colour;
    //
    // o_Image[pixelCoord] = float4(object_colour * (ambient + diffuse), 1);
    o_Image[pixelCoord] = float4(hit.colour, 1);
  } else {
    o_Image[pixelCoord] = float4(float3(0.2), 1);
  }
#endif
}
