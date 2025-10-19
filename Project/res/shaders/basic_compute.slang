#include "push_constants.slang"
#include "ray.slang"

struct Sphere {
  float3 origin;
  float radius;
};

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> image;

[[vk::binding(1, 0)]]
StructuredBuffer<Sphere> buffer;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  int width, height;
  image.GetDimensions(width, height);
  float aspect = float(width) / float(height);

  float2 uv = pixelCoord / float2(width, height);

  const float viewport_width = 2.0f;
  const float viewport_height = viewport_width / aspect;
  const float viewport_distance = 1.f;

  const float3 top_left = viewport_distance * push_constants.camera_front
                          - push_constants.camera_right * (viewport_width / 2.)
                          + push_constants.camera_up * (viewport_height / 2.);

  const float3 right_length = push_constants.camera_right * viewport_width;
  const float3 down_length = -push_constants.camera_up * viewport_height;

  float3 target = normalize(top_left + right_length * uv.x + down_length * uv.y);

  Ray ray = Ray(push_constants.camera_position, target);
  HitRecord record;

  Sphere spheres[5];
  for (int i = 0; i < 5; i++) {
    spheres[i].origin = float3(i*2, 0, 2);
    spheres[i].radius = 1.0;
  }

  for (int i = 0; i < buffer.getCount(); i++) {
    HitRecord newRecord = sphereIntersection(ray, buffer[i].origin, buffer[i].radius);

    if (!(record.hit)) {
      record = newRecord;
    } else if (newRecord.t < record.t) {
      record = newRecord;
    }
  }

  if (record.hit) {
    float3 colour = abs(record.normal);
    image[pixelCoord] = float4(colour, 1.0);
  } else {
    image[pixelCoord] = float4(float3(0.0), 1.0);
  }
}
