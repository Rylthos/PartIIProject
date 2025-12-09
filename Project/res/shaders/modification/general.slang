#pragma once

enum {
  OP_SET   = 1 << 0,
  OP_ERASE = 1 << 1,
};

struct ModStruct {
  int mod_type;

  uint3 voxel_index;
  float3 colour;
}
