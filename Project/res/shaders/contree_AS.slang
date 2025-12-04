#include "ray.slang"
#include "hit_record.slang"
#include "lighting.slang"
#include "default_defines.slang"
#include "render_descriptor.slang"
#include "intersection_colour.slang"

struct PushConstants {
  float3 camera_position;
  float4x4 contree_world;
  float4x4 contree_world_inverse;
  float4x4 contree_scale_inverse;
};

struct ContreeNode {
  uint64_t high;
  uint64_t low;

  property bool isSolid {
    get { return ((high >> 56) & 0x1) != 0; }
  }

  // Nodes
  property float nodeR {
    get { return ((high >> 16) & 0xFF) / 255.; }
  }

  property float nodeG {
    get { return ((high >> 8) & 0xFF) / 255.; }
  }

  property float nodeB {
    get { return ((high >> 0) & 0xFF) / 255.; }
  }

  property float3 nodeColour {
    get { return float3(nodeR, nodeG, nodeB); }
  }

  property uint32_t offset {
    get { return (uint32_t)(high & 0xFFFFFFFF); }
  }

  property uint64_t childMask {
    get { return low; }
  }

  // Leafs
  property float r {
    get { return (high & 0xFFFFFFFF) / (float)0xFFFF; }
  }

  property float g {
    get { return ((low >> 32) & 0xFFFFFFFF) / (float)0XFFFF; }
  }

  property float b {
    get { return (low & 0xFFFFFFFF) / (float)0xFFFF; }
  }

  property float3 colour {
    get { return float3(r, g, b); }
  }
}

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 1)]]
StructuredBuffer<ContreeNode> i_Contree;

func scaleExpToFloat(uint scale_exp) -> float {
    return asfloat((127 << 23) + (1 << scale_exp)) - 1.;
}

func floorScale(float3 value, uint scale_exp) -> float3 {
  return asfloat(asuint(value) & (~0u << scale_exp));
}

func calculateChildIndex(float3 position, uint scale_exp) -> uint {
  uint3 cell_pos = asuint(position) >> scale_exp & 3;
  return cell_pos.x + cell_pos.y * 4 + cell_pos.z * 16;
}

func countbits_64(uint64_t value, uint shift) -> uint {
uint himask = uint(value);
    uint count = 0;

    if (shift >= 32) {
        count = countbits(himask);
        himask = uint(value >> 32);
    }
    uint m = 1u << (shift & 31u);
    count += countbits(himask & (m - 1u));
    return count;
}

func traverse(in ray_original : Ray) -> HitRecord {
  const float3 min_bound = float3(1);
  const float3 max_bound = float3(2);

  const float4 origin_w = mul(float4(ray_original.origin, 1), push_constants.contree_world_inverse);
  const float4 direction_w = mul(float4(ray_original.direction, 1), push_constants.contree_scale_inverse);

  Ray ray = Ray(
    origin_w.xyz / origin_w.w,
    normalize(direction_w.xyz / direction_w.w),
  );

  HitRecord hit;
  float boundingTMin, boundingTMax;
  if (!ray.aabb(min_bound, max_bound, boundingTMin, boundingTMax, EPS, MAX_FLOAT)) {
    return hit;
  }

  uint stack[MAX_DEPTH];
  uint scale_exp = 21;

  uint node_index = 0;
  ContreeNode node = i_Contree[node_index];

  float3 position = clamp(ray.calculate(boundingTMin), min_bound, max_bound - EPS);
  float3 normal = calculateNormal(position, min_bound, max_bound);

const int3 step_dir = sign(ray.direction);

for (int i = 0; i < STEP_LIMIT; i++) {
#ifdef HEATMAP
    hit.intersection_checks++;
#endif

    uint child_index = calculateChildIndex(position, scale_exp);

    while (!node.isSolid && ((node.childMask >> child_index) & 1) != 0) {
      stack[scale_exp] = node_index;
#ifdef HEATMAP
      hit.intersection_checks++;
#endif

      uint offset = node.offset;
      uint child_offset = countbits_64(node.childMask, child_index);
      node_index += offset + child_offset;
      node = i_Contree[node_index];

      scale_exp -= 2;
      child_index = calculateChildIndex(position, scale_exp);
    };

    if (node.isSolid) {
      float4 pos = mul(float4(position, 1.), push_constants.contree_world);
      hit.hit = true;
      hit.hit_position = pos.xyz / pos.w;
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
    float3 parent_cell_min = floorScale(previous_position, scale_exp + 2);
    float3 new_cell_min = floorScale(position, scale_exp + 2);
    bool3 diff = abs(parent_cell_min - new_cell_min) > EPS;
    while (diff.x || diff.y || diff.z) {

#ifdef HEATMAP
      hit.intersection_checks++;
#endif

      scale_exp += 2;

      if (scale_exp >= MAX_DEPTH) {
        return hit;
      }

      node_index = stack[scale_exp];
      node = i_Contree[node_index];

      parent_cell_min = floorScale(previous_position, scale_exp + 2);
      new_cell_min = floorScale(position, scale_exp + 2);
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

  Ray ray = Ray(push_constants.camera_position, i_RayDirectionImage[pixelCoord].xyz);

  HitRecord hit;
#ifdef CONTREE_GENERATION_FINISHED
  hit = traverse(ray);
#endif

#ifdef CYCLES
  uint64_t end_cycles = clockARB();
#endif

#ifdef HEATMAP
  if (hit.intersection_checks != 0) {
    o_Image[pixelCoord] = float4(viridis(hit.intersection_checks), 1.);
  } else {
    o_Image[pixelCoord] = float4(0);
  }
#elif defined(CYCLES)
  float3 mix = lerp(MIN_CYCLE_COLOUR, MAX_CYCLE_COLOUR, float3((end_cycles - start_cycles) / float(CYCLE_MAX)));
  o_Image[pixelCoord] = float4(mix, 1.);
#else
  if (hit.hit) {
    o_Image[pixelCoord] = float4(calculateColour(hit, float3(5, 5, 0), float3(0.8)), 1);
    // o_Image[pixelCoord] = float4(hit.colour, 1);
  } else {
    o_Image[pixelCoord] = float4(0.2);
  }
#endif
}
