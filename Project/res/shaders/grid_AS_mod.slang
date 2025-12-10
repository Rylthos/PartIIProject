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

func validIndex(in voxel_index: int3) -> bool {
  return any(voxel_index >= 0) && any(voxel_index < push_constants.dimensions);
}

func getIndex(in voxel_index: int3) -> uint {
  return voxel_index.x +
    voxel_index.z * push_constants.dimensions.x +
    voxel_index.y * push_constants.dimensions.x * push_constants.dimensions.z;
}

func setVoxel(in voxel_index: int3, in place: bool) {
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

func sphere(in voxel_index: uint3) {
  int radius = int(push_constants.mod.additional.x);

  for (int y = -radius; y < radius; y++) {
    for (int z = -radius; z < radius; z++) {
      for (int x = -radius; x < radius; x++) {
        int3 newIndex = int3(x, y, z);
        if (!validIndex(voxel_index + newIndex)) continue;

        float dist = length(float3(newIndex));

        if (dist <= radius) {
          setVoxel(voxel_index + newIndex, true);
        }
      }
    }
  }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  uint3 voxel_index = push_constants.mod.voxel_index;

  if (!validIndex(voxel_index))
    return;

  if (push_constants.mod.mod_type == OP_SET) {
    setVoxel(voxel_index, true);
  } else if (push_constants.mod.mod_type == OP_ERASE) {
    setVoxel(voxel_index, false);
  } else if (push_constants.mod.mod_type == OP_SPHERE) {
    sphere(voxel_index);
  }
}
