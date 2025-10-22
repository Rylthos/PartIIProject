#include "hit_record.slang"

struct Ray {
  float3 origin;
  float3 direction;

  __init(float3 o, float3 d) { origin = o; direction = d; }


  __init(float aspect, float2 uv, float3 camera_pos, float3 camera_front, float3 camera_right, float3 camera_up) {
    const float viewport_width = 2.0;
    const float viewport_height = viewport_width / aspect;
    const float viewport_distance = 1.0;

    const float3 top_left = viewport_distance * camera_front
                          - camera_right * (viewport_width / 2.)
                          + camera_up * (viewport_height / 2.);

    const float3 right_length = camera_right * viewport_width;
    const float3 down_length = -camera_up * viewport_height;

    origin = camera_pos;
    direction = normalize(top_left + right_length * uv.x + down_length * uv.y);
  }

  float3 calculate(float t) {
    return origin + direction * t;
  }
};

HitRecord sphereIntersection(Ray ray, float3 center, float radius) {

  float3 p = ray.origin - center;
  float a = dot(ray.direction, ray.direction);
  float b = dot(p, ray.direction);
  float c = dot(p, p) - radius * radius;

  float disc = b*b - a*c;

  HitRecord record;
  record.hit = false;
  if (disc < 0.f) {
    return record;
  }

  float sqrt_b = sqrt(disc);
  float t = (-b - sqrt_b) / a;
  if (t < 0.f) {
    t = t + (-b + sqrt_b) / a;
  }

  if (t > 0.f) {
    record.hit = true;
    record.hit_position = ray.calculate(t);
    record.normal = normalize(record.hit_position - center);
    record.t = t;
  }

  return record;
}
