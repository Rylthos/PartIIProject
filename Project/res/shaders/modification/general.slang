#pragma once

enum
{
  SHAPE_VOXEL  = 0,
  SHAPE_SPHERE = 1,
  SHAPE_CUBE   = 2,
  SHAPE_CUBOID = 3,
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

func cuboid<T>(in center: uint3,
               in side_length: int3,
               in colour: float3,
               in place: bool,
               in forward: float3) -> void
  where T : IModification
{
  int3 forward_close = dot(forward.xz, float2(1, 0)) < (sqrt(2) / 2.) ? int3(0, 0, 1) : int3(1, 0, 0);

  int3 min_length = int3(floor(float3(side_length) / 2.));
  int3 max_length = int3(ceil(float3(side_length) / 2.));
  for (int y = -min_length.y; y < max_length.y; y++) {
    for (int z = -min_length.z; z < max_length.z; z++) {
      for (int x = -min_length.x; x < max_length.x; x++) {

        int nx = (forward_close.x == 0) ? z : x;
        int nz = (forward_close.x == 0) ? x : z;

        int3 newIndex = int3(nx, y, nz);
        if (!T.validIndex(center + newIndex))
          continue;

        T.setVoxel(center + newIndex, colour, place);
      }
    }
  }
}

func run_shape<T>(in voxel_index: uint3, in forward: float3, in mod: ModStruct) -> void
    where T : IModification
{
  if (!T::validIndex(voxel_index))
    return;

  bool place = mod.place == 1;

  if (mod.shape == SHAPE_VOXEL) {
    T::setVoxel(voxel_index, mod.colour, place);
  } else if (mod.shape == SHAPE_SPHERE) {
    sphere<T>(voxel_index, int(mod.additional.x),
        mod.colour, place);
  } else if (mod.shape == SHAPE_CUBE) {
    cuboid<T>(voxel_index, int3(mod.additional.x),
        mod.colour, place, forward);
  } else if (mod.shape == SHAPE_CUBOID) {
    cuboid<T>(voxel_index, int3(mod.additional.xyz),
        mod.colour, place, forward);
  }
}
