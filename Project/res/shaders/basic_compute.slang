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

func lightPixel(in record : HitRecord, in ray : Ray) -> float4 {
  const float3 light_colour = float3(1.0);
  const float ambient_strength = 0.15;
  float3 ambient = light_colour * ambient_strength;

  const float3 object_colour = float3(0.3, 0.55, 0.6);
  const float3 light_pos = float3(7.5, -10.0, 9.5);

  float3 light_dir = normalize(light_pos - record.hit_position);
  float diff = max(dot(record.normal, light_dir), 0.0);
  float3 diffuse = diff * light_colour;

  float3 result = (diffuse + ambient) * object_colour;

  return float4(result, 1.0);
}

func missColour(in ray : Ray) -> float4 {
  float mix = ray.direction.y;

  float4 top_colour = float4(0.3, 0.4, 0.9, 1.0);
  float4 bottom_colour = float4(0.1, 0.1, 0.1, 1.0);

  return lerp(top_colour, bottom_colour, float4(mix));
}

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

  for (int i = 0; i < buffer.getCount(); i++) {
    HitRecord newRecord = sphereIntersection(ray, buffer[i].origin, buffer[i].radius);

    if (!(record.hit)) {
      record = newRecord;
    } else if (newRecord.t < record.t) {
      record = newRecord;
    }
  }

  if (record.hit) {
    image[pixelCoord] = lightPixel(record, ray);
  } else {
    image[pixelCoord] = missColour(ray);
  }
}
