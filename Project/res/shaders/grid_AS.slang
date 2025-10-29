#include "push_constants.slang"
#include "ray.slang"
#include "default_defines.slang"

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

[[vk::binding(0, 1)]]
StructuredBuffer<uint8_t> i_Occupancy;

[[vk::binding(1, 1)]]
StructuredBuffer<float> i_Colour;

#define GRID_LENGTH 32

func traverse(in ray : Ray, in startingT : float) -> HitRecord {
  HitRecord hit;
  hit.hit = false;

  if (startingT < 0.) {
    return hit;
  }

#ifdef COUNT_INTERSECTIONS
  hit.intersection_checks = 0;
#endif

  float3 entry_pos = ray.calculate(startingT + EPS) / VOXEL_SIZE;

  int3 voxel_pos = int3(floor(entry_pos));
  if (voxel_pos.x < 0 || voxel_pos.y < 0 || voxel_pos.z < 0 ||
      voxel_pos.x >= GRID_LENGTH || voxel_pos.y >= GRID_LENGTH || voxel_pos.z >= GRID_LENGTH)
    return hit;

  voxel_pos = clamp(voxel_pos, int3(0), int3(GRID_LENGTH - 1));

  int3 step_dir = sign(ray.direction);
  float3 next_dist = abs((voxel_pos + max(step_dir, int3(0)) - entry_pos) * ray.inverse_direction);
  float3 step_size = ray.inverse_direction * step_dir;

  // Needs to be initialized;
  float3 normal;

  for (int i = 0; i < STEP_LIMIT; i++) {
#ifdef COUNT_INTERSECTIONS
  hit.intersection_checks += 1;
#endif

    uint index = voxel_pos.x + voxel_pos.y * GRID_LENGTH + voxel_pos.z * GRID_LENGTH * GRID_LENGTH;
    uint array_index = index / 8;
    uint bit_index = index % 8;

    bool is_occupied = bool((i_Occupancy[array_index] >> (7 - bit_index)) & 1);
    if (is_occupied) {
      hit.hit = true;
      int colour_index = index * 3;
      hit.colour.r = i_Colour[colour_index + 0];
      hit.colour.g = i_Colour[colour_index + 1];
      hit.colour.b = i_Colour[colour_index + 2];
      hit.normal = normal;
      return hit;
    }

    int3 step_axis = next_dist.xyz <= (min(next_dist.yzx, next_dist.zxy));

    next_dist +=  step_axis * step_size;
    voxel_pos +=  step_axis * step_dir;
    normal     = -step_axis * step_dir;

    // Outside bounds
    if (voxel_pos.x < 0 || voxel_pos.y < 0 || voxel_pos.z < 0 ||
        voxel_pos.x >= GRID_LENGTH || voxel_pos.y >= GRID_LENGTH || voxel_pos.z >= GRID_LENGTH)
      return hit;
  }

  return hit;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
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

  float startingT = ray.aabb(float3(0), float3(GRID_LENGTH * VOXEL_SIZE));
  HitRecord hit = traverse(ray, startingT);

#ifdef COUNT_INTERSECTIONS
    if (hit.intersection_checks != 0) {
      float3 mix = lerp(MIN_INTERSECTION_COLOUR, MAX_INTERSECTION_COLOUR, float3(hit.intersection_checks / float(INTERSECTION_MAX)));
      o_Image[pixelCoord] = float4(mix, 1.);
    } else {
      o_Image[pixelCoord] = float4(0);
    }
#else
    if (hit.hit) {
      o_Image[pixelCoord] = float4(hit.colour, 1.0);
    } else {
      o_Image[pixelCoord] = float4(float3(0.1), 1.0);
    }
#endif
}
