struct HitRecord {
  bool hit;
  float3 colour;
  float3 hit_position;
  float3 normal;
  float t;
#ifdef COUNT_INTERSECTIONS
  int intersection_checks;
#endif

  __init() {
    hit = false;
    colour = float3(0);
    hit_position = float3(0);
    normal = float3(0);
    t = 1.0 / 0.;
#ifdef COUNT_INTERSECTIONS
    intersection_checks = 0;
#endif
  }
}
