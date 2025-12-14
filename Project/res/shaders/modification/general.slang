#pragma once

enum
{
  SHAPE_VOXEL = 0,
  SHAPE_SPHERE = 1,
};

struct ModStruct
{
  int shape;
  int place;
  uint3 voxel_index;
  float3 colour;
  float4 additional;
}

interface IModification
{
  static bool validIndex(in int3);
  static void setVoxel(in int3, in float3, in bool);
}

func sphere<T>(in center : uint3,
               in radius : int,
               in colour : float3,
               in place : bool) -> void
   where T : IModification
{
  for (int y = -radius; y < radius; y++) {
    for (int z = -radius; z < radius; z++) {
      for (int x = -radius; x < radius; x++) {
        int3 newIndex = int3(x, y, z);
        if (!T.validIndex(center + newIndex))
          continue;

        float dist = length(float3(newIndex));

        if (dist <= radius) {
          T.setVoxel(center + newIndex, colour, place);
        }
      }
    }
  }
}
