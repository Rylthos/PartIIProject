#include "ray.slang"
#include "push_constants.slang"
#include "default_defines.slang"

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

func traverse(in ray : Ray) -> HitRecord {
  const float3 world_offset = float3(0);
  // const float3 world_dimensions = float3(2); // Not currently working

  float3 min_bound = float3(1);
  float3 max_bound = float3(3);

  const float3 dimensions = max_bound - min_bound;

  Ray transformed = Ray(
    (ray.origin + (min_bound - world_offset)),
    ray.direction
  );

  HitRecord hit;
  float boundingTMin, boundingTMax;
  if (!transformed.aabb(min_bound, max_bound, boundingTMin, boundingTMax, EPS, MAX_FLOAT)) {
    return hit;
  }

  float3 position = transformed.calculate(boundingTMin + EPS);

  int current_index = -1;
  StackMember stack[MAX_DEPTH];

  float current_scale = 0.5;
  uint parent = 0;

  OctreeNode node;
  float t = boundingTMin;

  float currentTMin = boundingTMin;
  float currentTMax = boundingTMax;

  for (int i = 0; i < STEP_LIMIT; i++) {
    node = new OctreeNode(i_Octree[parent]);

#ifdef HEATMAP
hit.intersection_checks += 1;
#endif

    if (node.isSolid) {
      hit.hit = true;
      hit.colour = node.colour;

      return hit;
    }

    if (t >= currentTMax) {
      // Ascend
      if (current_index == -1) break;

      currentTMax = stack[current_index].tMax;
      parent = stack[current_index].parent;
      min_bound = stack[current_index].minBound;
      current_index--;

      current_scale *= 2;

      continue;
    }

    float3 center = min_bound + current_scale * dimensions;
    float3 boundOffset = float3(0);
    int octantMask = 0;

    if (position.x > center.x || (position.x == center.x && transformed.direction.x > 0)) {
      octantMask ^= 1;
      boundOffset.x = dimensions.x;
    }

    if (position.z > center.z || (position.z == center.z && transformed.direction.z > 0)) {
      octantMask ^= 2;
      boundOffset.z = dimensions.z;
    }

    if (position.y > center.y || (position.y == center.y && transformed.direction.y > 0)) {
      octantMask ^= 4;
      boundOffset.y = dimensions.y;
    }

    bool childMask = ((node.childMask >> octantMask) & 1) == 1;

    if (childMask) { // Child exists and has content
      // Descend into child
      if (node.offset == 0) break;

      current_index++;
      stack[current_index].parent = parent;
      stack[current_index].tMax = currentTMax;
      stack[current_index].minBound = min_bound;

      uint child_offset = countbits(node.childMask >> (octantMask + 1));
      parent = parent + node.offset + child_offset;

      min_bound = min_bound + boundOffset * current_scale;
      max_bound = min_bound + dimensions * current_scale;

      const float minT = currentTMin;
      const float maxT = currentTMax;
      if (!transformed.aabb(min_bound, max_bound, currentTMin, currentTMax, minT, maxT))
        break;

      current_scale *= 0.5;
    } else { // Child is empty
      // Traverse through air

      float3 octantMinBound = min_bound + boundOffset * current_scale;
      float3 octantMaxBound = octantMinBound + current_scale * dimensions;

      float t0;
      if (!transformed.aabb(octantMinBound, octantMaxBound, t0, t, currentTMin, currentTMax))
        break;

      t += EPS;
      position = transformed.calculate(t);
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
    o_Image[pixelCoord] = float4(hit.colour, 1.);
  } else {
    o_Image[pixelCoord] = float4(float3(0.2), 1);
  }
#endif
}
