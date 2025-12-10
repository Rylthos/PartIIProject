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

func getIndex(in voxel_index: int3) -> uint {
  return voxel_index.x +
    voxel_index.z * push_constants.dimensions.x +
    voxel_index.y * push_constants.dimensions.x * push_constants.dimensions.z;
}

struct GridModification : IModification {
  static func validIndex(in voxel_index: int3) -> bool {
    return any(voxel_index >= 0) && any(voxel_index < push_constants.dimensions);
  }

  static func setVoxel(in voxel_index: int3, in colour: float3) -> void {
    uint index = getIndex(voxel_index);

    uint array_index = index / 32;
    uint bit_index = index % 32;

    i_Occupancy[array_index] |= 1 << bit_index;

    float3 colour = push_constants.mod.colour;
    i_Colour[index * 3 + 0] = colour.r;
    i_Colour[index * 3 + 1] = colour.g;
    i_Colour[index * 3 + 2] = colour.b;
  }

  static func eraseVoxel(in voxel_index: int3) -> void {
    uint index = getIndex(voxel_index);

    uint array_index = index / 32;
    uint bit_index = index % 32;

    i_Occupancy[array_index] &= ~(1 << bit_index);
  }
}


[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  uint3 voxel_index = push_constants.mod.voxel_index;

  if (!GridModification::validIndex(voxel_index))
    return;

  if (push_constants.mod.mod_type == OP_SET) {
    GridModification::setVoxel(voxel_index, push_constants.mod.colour);
  } else if (push_constants.mod.mod_type == OP_ERASE) {
    GridModification::eraseVoxel(voxel_index);
  } else if (push_constants.mod.mod_type == OP_SPHERE) {
    sphere<GridModification>(voxel_index, int(push_constants.mod.additional.x), push_constants.mod.colour);
  }
}
