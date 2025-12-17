#include "general.slang"

#include "../AS/structures/brickmap.slang"

struct PushConstants
{
  uint3 dimensions;

  float3 camera_facing;

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
    uint brickgrid_index = getBrickIndex(mod_index);

    Brickgrid grid = i_Brickgrid[brickgrid_index];

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

      i_Brickgrid[brickgrid_index].getPtr = freeIndex;

      grid = i_Brickgrid[brickgrid_index];

      for (int i = 0; i < 8; i++) {
        i_Brickmap[grid.getPtr].data[i] = 0;
      }

      i_Brickgrid[brickgrid_index].isValid = true;
    }

    if (grid.isValid) {
      uint3 voxel_index = getVoxelIndex(mod_index);

      uint mask_index = voxel_index.x + voxel_index.z * 8;

      bool possiblyEmpty = false;
      if (place) {
        i_Brickmap[grid.getPtr].data[voxel_index.y] |= ((uint64_t)1 << mask_index);
      } else {
        i_Brickmap[grid.getPtr].data[voxel_index.y] &= ~((uint64_t)1 << mask_index);
        possiblyEmpty = (i_Brickmap[grid.getPtr].data[voxel_index.y] == 0);
      }

      if (possiblyEmpty) {
        uint count = 0;
        for (int i = 0; i < 8; i++) {
          count += countbits(i_Brickmap[grid.getPtr].data[i]);
        }

        if (count == 0) {
          uint brick_index = i_Brickgrid[brickgrid_index].getPtr;
          i_Brickgrid[brickgrid_index].isEmpty = true;

          int size, stride;
          i_FreeBricks.GetDimensions(size, stride);

          for (int i = 1; i < size; i++) {
            if (i_FreeBricks[i] == 0) {

              i_FreeBricks[i] = brick_index + 1;
              i_FreeBricks[0] += 1;

              break;
            }
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

  run_shape<BrickmapModification>(voxel_index, push_constants.camera_facing, push_constants.mod);
}
