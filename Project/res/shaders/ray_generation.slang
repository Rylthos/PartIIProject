#include "setup_descriptor.slang"
#include "ray.slang"

struct PushConstants {
  float3 camera_position;
  float3 camera_front;
  float3 camera_right;
  float3 camera_up;
}

[[vk_push_constant]]
PushConstants push_constants;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  int width, height;
  o_RayDirectionImage.GetDimensions(width, height);
  float aspect = float(width) / float(height);

  float2 uv = pixelCoord / float2(width, height);

  Ray ray = Ray(aspect,
                uv,
                push_constants.camera_position,
                push_constants.camera_front,
                push_constants.camera_right,
                push_constants.camera_up
  );

  o_RayDirectionImage[pixelCoord] = float4(ray.direction, 1.);
}
