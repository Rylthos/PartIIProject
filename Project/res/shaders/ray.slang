#pragma once
#include "hit_record.slang"

#include "default_defines.slang"

struct Ray
{
  float3 origin;
  float3 direction;
  float3 inverse_direction;

  __init(float3 o, float3 d) {
    origin = o;
    direction = d;
    inverse_direction = 1. / direction;
  }

  __init(float aspect,
         float2 uv,
         float3 camera_pos,
         float3 camera_front,
         float3 camera_right,
         float3 camera_up)
  {
    const float viewport_width = 2.0;
    const float viewport_height = viewport_width / aspect;
    const float viewport_distance = 1.0;

    const float3 top_left = viewport_distance * camera_front -
                            camera_right * (viewport_width / 2.) +
                            camera_up * (viewport_height / 2.);

    const float3 right_length = camera_right * viewport_width;
    const float3 down_length = -camera_up * viewport_height;

    origin = camera_pos;
    direction = normalize(top_left + right_length * uv.x + down_length * uv.y);
    inverse_direction = 1. / direction;
  }

  float3 calculate(float t) { return origin + direction * t; }

  bool aabb(float3 minBound,
            float3 maxBound,
            out float tMin,
            out float tMax,
            in float minT,
            in float maxT)
  {
    float3 t_bot = this.inverse_direction * (minBound - this.origin);
    float3 t_top = this.inverse_direction * (maxBound - this.origin);

    float3 t_min = min(t_top, t_bot);
    float3 t_max = max(t_top, t_bot);

    float2 t = max(t_min.xx, t_min.yz);
    float t0 = max(t.x, t.y);
    t = min(t_max.xx, t_max.yz);
    float t1 = min(t.x, t.y);

    tMin = max(t0, minT);
    tMax = min(t1, maxT);

    return t1 > max(t0, 0.) && tMax > minT && tMin < maxT;
  }

  float aabb(float3 minBound, float3 maxBound)
  {
    float3 t_all_min = (minBound - this.origin) * inverse_direction;
    float3 t_all_max = (maxBound - this.origin) * inverse_direction;

    float3 t_min = min(t_all_min, t_all_max);
    float3 t_max = max(t_all_min, t_all_max);

    float2 t = max(t_min.xx, t_min.yz);
    float t0 = max(t.x, t.y);

    t = min(t_max.xx, t_max.yz);
    float t1 = min(t.x, t.y);

    float tMin = max(t0, 0);
    float tMax = min(t1, MAX_FLOAT);

    if (tMax > tMin) {
      return tMin;
    }

    return -1;
  }
};

func createShadowRay(in hit : HitRecord, in light_pos : float3) -> Ray
{
  const float3 origin = hit.voxel_index + float3(0.5) + hit.normal / 2.;
  return Ray(origin, light_pos - origin);
}

HitRecord sphereIntersection(Ray ray, float3 center, float radius)
{
  float3 p = ray.origin - center;
  float a = dot(ray.direction, ray.direction);
  float b = dot(p, ray.direction);
  float c = dot(p, p) - radius * radius;

  float disc = b * b - a * c;

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

float3 calculateNormal(float3 position, float3 minBound, float3 maxBound)
{
  float3 minSide = abs(position - minBound);
  float3 maxSide = abs(position - maxBound);

  float3 negative = select(minSide < EPS, float3(-1), float3(0));
  float3 positive = select(maxSide < EPS, float3(1), float3(0));

  return negative + positive;
}
