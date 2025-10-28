#include "hit_record.slang"

struct Ray {
  float3 origin;
  float3 direction;
  float3 inverse_direction;

  __init(float3 o, float3 d) { origin = o; direction = d; inverse_direction = 1. / direction; }


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
    inverse_direction = 1. / direction;
  }

  float3 calculate(float t) {
    return origin + direction * t;
  }

  bool aabb(float3 minBound, float3 maxBound, out float tMin, out float tMax, in float minT, in float maxT) {
    float3 t_bot = this.inverse_direction * (minBound - this.origin);
    float3 t_top = this.inverse_direction * (maxBound - this.origin);

    float3 t_min = min(t_top, t_bot);
    float3 t_max = max(t_top, t_bot);

    float2 t = max(t_min.xx, t_min.yz);
    float t0 = max(t.x, t.y);
    t = min(t_max.xx, t_max.yz);
    float t1 = min(t.x, t.y);

    tMin = t0;
    tMax = t1;

    return t1 > max(t0, 0.) && tMax > minT && tMin < maxT;
  }

  float aabb(float3 c1, float3 c2) {
    float3 minB = min(c1, c2);
    float3 maxB = max(c1, c2);

    float tx1 = (minB.x - origin.x) * inverse_direction.x;
    float tx2 = (maxB.x - origin.x) * inverse_direction.x;

    float tmin = min(tx1, tx2);
    float tmax = max(tx1, tx2);

    float ty1 = (minB.y - origin.y) * inverse_direction.y;
    float ty2 = (maxB.y - origin.y) * inverse_direction.y;

    tmin = max(tmin, min(ty1, ty2));
    tmax = min(tmax, max(ty1, ty2));

    float tz1 = (minB.z - origin.z) * inverse_direction.z;
    float tz2 = (maxB.z - origin.z) * inverse_direction.z;

    tmin = max(tmin, min(tz1, tz2));
    tmax = min(tmax, max(tz1, tz2));

    if (tmax >= tmin) return max(tmin, 0.);
    return -1.f;
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
