#pragma once

#include "hit_record.slang"

struct HitData {
  float4 hit_position;
  int4 voxel_index;
  float4 normal;
  int hit;
};

func setHitRecord(in data : HitData *, in hit : HitRecord) -> void
{
  data.hit_position = float4(hit.hit_position, 1);
  data.voxel_index = int4(hit.voxel_index, 1);
  data.normal = float4(hit.normal, 1);
  data.hit = hit.hit;
}
