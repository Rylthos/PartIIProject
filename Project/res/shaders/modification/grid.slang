#include "general.slang"

struct PushConstants {
  uint3 dimensions;

  ModStruct mod;
}

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWStructuredBuffer<uint32_t> i_Occupancy;

[[vk::binding(1, 0)]]
RWStructuredBuffer<float> i_Colour;

func getIndex(in voxel_index : int3) -> uint {
  return voxel_index.x + voxel_index.z * push_constants.dimensions.x +
    voxel_index.y * push_constants.dimensions.x * push_constants.dimensions.z;
}

struct GridModification : IModification
{
  static func validIndex(in voxel_index : int3) -> bool
  {
    return all(voxel_index >= 0) && all(voxel_index < push_constants.dimensions);
  }

  static func setVoxel(in voxel_index : int3, in colour : float3, in place : bool) -> void
  {
    uint index = getIndex(voxel_index);

    uint array_index = index / 32;
    uint bit_index = index % 32;

    if (place) {
      i_Occupancy[array_index] |= 1 << bit_index;

      float3 colour = push_constants.mod.colour;
      i_Colour[index * 3 + 0] = colour.r;
      i_Colour[index * 3 + 1] = colour.g;
      i_Colour[index * 3 + 2] = colour.b;
    } else {
      i_Occupancy[array_index] &= ~(1 << bit_index);
    }
  }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  uint3 voxel_index = push_constants.mod.voxel_index;

  if (!GridModification::validIndex(voxel_index))
    return;

  bool place = push_constants.mod.place == 1;

  if (push_constants.mod.shape == SHAPE_VOXEL) {
    GridModification::setVoxel(voxel_index, push_constants.mod.colour, place);
  } else if (push_constants.mod.shape == SHAPE_SPHERE) {
    sphere<GridModification>(voxel_index, int(push_constants.mod.additional.x),
        push_constants.mod.colour, place);
  }
}
