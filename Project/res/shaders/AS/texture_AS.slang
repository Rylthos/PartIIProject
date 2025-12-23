#define EPS 1e-4

#include "../default_defines.slang"
#include "../hit_record.slang"
#include "../ray.slang"
#include "../render_descriptor.slang"
#include "general.slang"

struct PushConstants
{
  float3 camera_position;
  uint3 dimensions;
  uint64_t hit_data_address;
};

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 1)]]
RWTexture3D<float4> i_VoxelData;

struct TextureRayMarch : IRayMarch
{
  static func traverse(in ray : Ray) -> HitRecord
  {
    const float3 minBound = float3(0);
    const float3 maxBound = minBound + float3(push_constants.dimensions * VOXEL_SIZE);

    HitRecord hit;

    float startingT = ray.aabb(minBound, maxBound);

    if (startingT < 0.) {
      return hit;
    }

    #ifdef HEATMAP
    hit.intersection_checks = 0;
    #endif

    float3 entry_pos = ray.calculate(startingT + EPS) / VOXEL_SIZE;

    int3 voxel_pos = clamp(int3(floor(entry_pos)), int3(0), int3(push_constants.dimensions - 1));

    int3 step_dir = sign(ray.direction);
    float3 next_dist = abs((voxel_pos + max(step_dir, int3(0)) - entry_pos) * ray.inverse_direction);
    float3 step_size = ray.inverse_direction * step_dir;

    float3 normal = calculateNormal(entry_pos, minBound, maxBound);

    float t = startingT;

    for (int i = 0; i < STEP_LIMIT; i++) {
      #ifdef HEATMAP
      hit.intersection_checks += 1;
      #endif

      bool is_occupied = i_VoxelData[voxel_pos].a > 0;
      if (is_occupied) {
        hit.hit = true;
        hit.normal = normal;
        hit.hit_position = ray.calculate(t);

        hit.voxel_index = voxel_pos;

        hit.colour.rgb = i_VoxelData[voxel_pos].rgb;

        return hit;
      }

      int3 step_axis = next_dist.xyz <= (min(next_dist.yzx, next_dist.zxy));

      t = startingT + dot(next_dist, step_axis);

      next_dist += step_axis * step_size;
      voxel_pos += step_axis * step_dir;
      normal = -step_axis * step_dir;

      // Outside bounds
      if (any(voxel_pos < 0) || any(voxel_pos >= push_constants.dimensions))
        return hit;
    }

    return hit;
  }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  int2 pixel_coord = dispatchThreadID.xy;

  int width, height;
  i_RayDirectionImage.GetDimensions(width, height);

  o_Image[pixel_coord] = render<TextureRayMarch>(pixel_coord, push_constants.camera_position,
      i_RayDirectionImage[pixel_coord].xyz, int2(width, height), push_constants.hit_data_address);
}
