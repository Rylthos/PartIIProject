#pragma once

struct HitRecord {
  bool hit;
  float3 colour;
  float3 hit_position;
  int3 voxel_index;
  float3 normal;
  float t;
#ifdef HEATMAP
  int intersection_checks;
#endif

  __init() {
    hit = false;
    colour = float3(0);
    hit_position = float3(0);
    normal = float3(0);
    voxel_index = int3(-1);
    t = 1.0 / 0.;
#ifdef HEATMAP
    intersection_checks = 0;
#endif
  }
}
