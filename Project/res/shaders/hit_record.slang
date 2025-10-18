struct HitRecord {
  bool hit;
  float3 hit_position;
  float3 normal;
  float t;

  __init() {
    hit = false;
    hit_position = float3(0);
    normal = float3(0);
    t = 1.0 / 0.;
  }
}
