#include "push_constants.slang"
#include "ray.slang"

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

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

  HitRecord hit = sphereIntersection(ray, float3(0.0, 0.0, 2.0), 1.0);

  if (hit.hit) {
    o_Image[pixelCoord] = float4(abs(hit.normal), 1.0);
  } else {
    o_Image[pixelCoord] = float4(0.0, 0.0, 0.0, 1.0);
  }
}
