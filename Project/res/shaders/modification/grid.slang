#include "general.slang"

struct PushConstants {
  uint3 dimensions;
  float3 camera_forward;

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

  static func setVoxel(in voxel_index : int3, in colour : float3, in type : Type) -> void
  {
    uint index = getIndex(voxel_index);

    uint array_index = index / 32;
    uint bit_index = index % 32;

    switch (type) {
      case Type::PLACE:
        i_Occupancy[array_index] |= 1 << bit_index;

        float3 colour = push_constants.mod.colour;
        i_Colour[index * 3 + 0] = colour.r;
        i_Colour[index * 3 + 1] = colour.g;
        i_Colour[index * 3 + 2] = colour.b;
        break;
      case Type::ERASE:
        i_Occupancy[array_index] &= ~(1 << bit_index);
        break;
    }
  }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  uint3 voxel_index = push_constants.mod.voxel_index;

  run_shape<GridModification>(voxel_index, push_constants.camera_forward, push_constants.mod);
}
