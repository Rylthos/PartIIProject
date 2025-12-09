#include "modification/general.slang"

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

func setVoxel() {
  uint3 voxel_index = push_constants.mod.voxel_index;
  uint index = voxel_index.x +
  voxel_index.z * push_constants.dimensions.x +
  voxel_index.y * push_constants.dimensions.x * push_constants.dimensions.z;

  uint array_index = index / 32;
  uint bit_index = index % 32;

  i_Occupancy[array_index] |= 1 << bit_index;

  float3 colour = push_constants.mod.colour;
  i_Colour[index * 3 + 0] = colour.r;
  i_Colour[index * 3 + 1] = colour.g;
  i_Colour[index * 3 + 2] = colour.b;
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  if (push_constants.mod.mod_type == OP_SET) {
    setVoxel();
  }
}
