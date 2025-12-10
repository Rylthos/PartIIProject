#pragma once

enum {
  OP_SET   = 1 << 0,
  OP_ERASE = 1 << 1,
  OP_SPHERE = 1 << 2,
};

struct ModStruct {
  int mod_type;

  uint3 voxel_index;
  float3 colour;
  float4 additional;
}

interface IModification {
  static bool validIndex(in int3);
  static void setVoxel(in int3, in float3);
  static void eraseVoxel(in int3);
}

func sphere<T>(in center: uint3, in radius: int, in colour: float3) -> void
  where T: IModification
{
  for (int y = -radius; y < radius; y++) {
    for (int z = -radius; z < radius; z++) {
      for (int x = -radius; x < radius; x++) {
        int3 newIndex = int3(x, y, z);
        if (!T.validIndex(center + newIndex)) continue;

        float dist = length(float3(newIndex));

        if (dist <= radius) {
         T.setVoxel(center + newIndex, colour);
        }
      }
    }
  }
}
