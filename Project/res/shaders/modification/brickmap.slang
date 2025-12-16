#include "general.slang"

#include "../AS/structures/brickmap.slang"

struct PushConstants
{
  uint3 dimensions;

  ModStruct mod;
}

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWStructuredBuffer<Brickgrid> i_Brickgrid;

[[vk::binding(1, 0)]]
RWStructuredBuffer<Brickmap> i_Brickmap;

[[vk::binding(2, 0)]]
StructuredBuffer<uint8_t> i_Colours;

[[vk::binding(3, 0)]]
RWStructuredBuffer<uint32_t> i_Request;

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint32_t> i_FreeBricks;

func getBrickIndex(in index : int3) -> uint {
  uint3 brick = index / 8;
  return brick.x + brick.z * push_constants.dimensions.x +
    brick.y * push_constants.dimensions.x * push_constants.dimensions.z;
}

func getVoxelIndex(in index: int3) -> uint3 {
  return index % 8;
}

struct BrickmapModification : IModification
{
  static func validIndex(in voxel_index : int3) -> bool
  {
    return all(voxel_index >= 0) && all(voxel_index < push_constants.dimensions * 8);
  }

  static func setVoxel(in mod_index : int3, in colour : float3, in place : bool) -> void
  {
    uint index = getBrickIndex(mod_index);

    Brickgrid grid = i_Brickgrid[index];

    if (!grid.isValid || grid.isEmpty) {
      int freeIndex = -1;

      int size = i_FreeBricks[0];

      for (int i = 0; i < size; i++) {
        if (i_FreeBricks[i + 1] != 0) {

          freeIndex = i_FreeBricks[i + 1] - 1;
          i_FreeBricks[i + 1] = 0;
          i_FreeBricks[0] -= 1;

          break;
        }
      }

      i_Brickgrid[index].getPtr = freeIndex;

      for (int i = 0; i < 8; i++) {
        i_Brickmap[grid.getPtr].data[i] = 0;
      }

      i_Brickgrid[index].isValid = true;
    }

    if (grid.isValid && !grid.isEmpty) {
      uint3 voxel_index = getVoxelIndex(mod_index);

      uint mask_index = voxel_index.x + voxel_index.z * 8;

      if (place) {
        i_Brickmap[grid.getPtr].data[voxel_index.y] |= ((uint64_t)1 << mask_index);
      } else {
        i_Brickmap[grid.getPtr].data[voxel_index.y] &= ~((uint64_t)1 << mask_index);
      }

      uint count = 0;
      for (int i = 0; i < 8; i++) {
        count += countbits(i_Brickmap[grid.getPtr].data[i]);
      }

      if (count == 0) {
        uint index = i_Brickgrid[index].getPtr;

        i_Brickgrid[index].isEmpty = true;

        int size, stride;
        i_FreeBricks.GetDimensions(size, stride);

        for (int i = 0; i < size - 1; i++) {
          if (i_FreeBricks[i + 1] == 0) {

            i_FreeBricks[i + 1] = index + 1;
            i_FreeBricks[0] += 1;
          }
        }
      }
    }
  }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  uint3 voxel_index = push_constants.mod.voxel_index;

  if (!BrickmapModification::validIndex(voxel_index))
    return;

  bool place = push_constants.mod.place == 1;

  if (push_constants.mod.shape == SHAPE_VOXEL) {
    BrickmapModification::setVoxel(voxel_index, push_constants.mod.colour, place);
  } else if (push_constants.mod.shape == SHAPE_SPHERE) {
    sphere<BrickmapModification>(voxel_index, int(push_constants.mod.additional.x),
        push_constants.mod.colour, place);
  }
}
