#include "hit_record.slang"

struct Ray {
  float3 origin;
  float3 direction;

  __init(float3 o, float3 d) { origin = o; direction = d; }
};

float3 calculate(Ray ray, float t) {
  return (ray.origin + ray.direction * t);
}

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
    record.hit_position = calculate(ray, t);
    record.normal = normalize(record.hit_position - center);
    record.t = t;
  }

  return record;
}
