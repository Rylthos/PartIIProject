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
RWStructuredBuffer<Colour> i_Colours;

[[vk::binding(3, 0)]]
RWStructuredBuffer<uint32_t> i_Request;

[[vk::binding(0, 1)]]
RWStructuredBuffer<uint32_t> i_FreeBricks;

[[vk::binding(1, 1)]]
RWStructuredBuffer<uint32_t> i_FreeColours;

func getBrickIndex(in index : int3) -> uint {
  uint3 brick = index / 8;
  return brick.x + brick.z * push_constants.dimensions.x +
    brick.y * push_constants.dimensions.x * push_constants.dimensions.z;
}

func getVoxelIndex(in index: int3) -> uint3 {
  return index % 8;
}

func insertColour(in colour: float3, in target_index: uint, in colour_ptr: uint64_t, in total_colours: uint) -> void {
  for (int i = total_colours - 1; i > target_index; i--) {
    uint64_t index = colour_ptr + i;
    i_Colours[index].copyFrom(i_Colours[index - 1]);
  }

  i_Colours[colour_ptr + target_index].r = (uint8_t)floor(colour.r * 255.);
  i_Colours[colour_ptr + target_index].g = (uint8_t)floor(colour.g * 255.);
  i_Colours[colour_ptr + target_index].b = (uint8_t)floor(colour.b * 255.);
}

func eraseColour(in target_index: uint, in colour_ptr: uint64_t, in total_colours: uint) -> void {
  for (int i = target_index; i < total_colours; i++) {
    uint64_t index = colour_ptr + i;
    i_Colours[index].copyFrom(i_Colours[index + 1]);
  }
}

func findBlock(in size: uint) -> uint64_t {
  uint8_t type;
  if (size <= 2*2*2) {
    type = 2;
  } else if (size <= 4*4*4) {
    type = 1;
  } else {
    type = 0;
  }

  // Traverse free bricks
  if (type == 0) {
    int size, stride;
    i_FreeColours.GetDimensions(size, stride);

    for (int i = 1; i < size; i++) {
      if (i_FreeColours[i] != 0) {
        uint64_t colour_ptr = uint64_t(i_FreeColours[i]);
        i_FreeColours[i] = 0;
        i_FreeColours[0] -= 1;

        uint64_t final_ptr = (colour_ptr - 1) * 512;
        if (i_Colours[final_ptr].isUsed || i_Colours[final_ptr].type != type) {
          continue;
        }

        return final_ptr;
      }
    }
  }

  uint colour_size, stride;
  i_Colours.GetDimensions(colour_size, stride);

  for (uint64_t i = 0; i < colour_size;) {
    Colour c = i_Colours[i];

    if (c.type > type) {
      int offset = getTypeSize(c.type);
      i += offset;

      continue;
    }

    if (c.isUsed) {
      i += getTypeSize(c.type);

      continue;
    }

    if (c.type == type) { // Use as is
      return i;
    } else if (c.type < type) { // Split
      uint8_t newType = c.type + 1;
      uint type_size = getTypeSize(newType);
      i_Colours[i].type = newType;
      for (int j = 0; j < 8; j++) {
        i_Colours[i + j * type_size].type = newType;
      }
    }
  }

  return 0;
}

inline func copyColours(uint64_t start, uint64_t length, uint64_t target) -> void {
  for (uint64_t i = 0; i <= length; i++) {
    i_Colours[target + i].copyFrom(i_Colours[start + i]);
  }
}

func tryMerge(in colour_block: uint64_t) -> void {

  uint64_t blocks[2];

  int index = 0;
  blocks[index] = colour_block;

  while (index >= 0) {
    uint64_t block = blocks[index];

    if (i_Colours[colour_block].isUsed)
      return;

    uint8_t type = i_Colours[colour_block].type;

    if (type == 0)
      return;

    uint64_t block_start = colour_block;

    if (type == 1) { // 64
      block_start = (colour_block / 512) * 512;
    } else { // 8
      block_start = (block_start / 8) * 8;
    }

    uint offset = getTypeSize(type);

    for (uint8_t i = 0; i < 8; i++) {
      if (i_Colours[block_start + i * offset].isUsed)
        return;

      if (i_Colours[block_start + i * offset].type != type) {
        blocks[index + 1] = block_start + i * offset;
        continue;
      }
    }

    for (uint8_t i = 0; i < 8; i++) {
      i_Colours[block_start + i * offset].type = 0;
    }

    i_Colours[block_start].type = type - 1;

    index--;
  }
}

struct BrickmapModification : IModification
{
  static func validIndex(in voxel_index : int3) -> bool
  {
    return all(voxel_index >= 0) && all(voxel_index < push_constants.dimensions * 8);
  }

  static func setVoxel(in mod_index : int3, in colour : float3, in type : Type) -> void
  {
    uint brickgrid_index = getBrickIndex(mod_index);

    Brickgrid grid = i_Brickgrid[brickgrid_index];

    if ((!grid.isValid || grid.isEmpty) && type != Type::PLACE)
      return;

    if (!grid.isValid || grid.isEmpty) {
      int32_t freeIndex = -1;

      int size, stride;
      i_FreeBricks.GetDimensions(size, stride);

      for (int i = 1; i < size; i++) {
        if (i_FreeBricks[i] != 0) {
          freeIndex = i_FreeBricks[i] - 1;
          i_FreeBricks[i] = 0;
          i_FreeBricks[0] -= 1;

          break;
        }
      }

      i_Brickgrid[brickgrid_index].getPtr = freeIndex;

      for (int i = 0; i < 8; i++) {
        i_Brickmap[freeIndex].data[i] = 0;
      }

      i_Brickgrid[brickgrid_index].isValid = true;

      uint64_t colour_ptr = findBlock(1);
      i_Colours[colour_ptr].isUsed = true;
      i_Brickmap[freeIndex].colourPtr = colour_ptr;

      grid = i_Brickgrid[brickgrid_index];
    }

    Brickmap brick = i_Brickmap[grid.getPtr];

    uint3 voxel_index = getVoxelIndex(mod_index);

    uint mask_index = voxel_index.x + voxel_index.z * 8;

    uint oldcount = getAllSet(brick);

    bool possiblyEmpty = false;
    switch (type) {
      case Type::PLACE: {
                          uint64_t new_value = i_Brickmap[grid.getPtr].data[voxel_index.y] | ((uint64_t)1 << mask_index);

                          if (new_value == i_Brickmap[grid.getPtr].data[voxel_index.y]) {
                            break;
                          }

                          i_Brickmap[grid.getPtr].data[voxel_index.y] = new_value;

                          uint bitcount = getSetBits(i_Brickmap[grid.getPtr], voxel_index);

                          uint64_t colour_ptr = i_Brickmap[grid.getPtr].colourPtr;
                          uint8_t type = i_Colours[colour_ptr].type;
                          uint type_size = getTypeSize(type);

                          if (oldcount + 1 <= type_size) {
                            // We have enough space to remain using current position
                            insertColour(colour, bitcount, i_Brickmap[grid.getPtr].colourPtr, oldcount + 1);
                          } else {
                            // Find a new location
                            uint64_t newLocation = findBlock(oldcount + 1);
                            i_Colours[newLocation].isUsed = true;

                            copyColours(colour_ptr, oldcount, newLocation);
                            insertColour(colour, bitcount, newLocation, oldcount + 1);

                            i_Brickmap[grid.getPtr].colourPtr = newLocation;
                            i_Colours[colour_ptr].isUsed = false;
                          }

                          break;
                        }
      case Type::REPLACE: {
                            if ((i_Brickmap[grid.getPtr].data[voxel_index.y] & ((uint64_t)1 << mask_index)) != 0) {
                              uint64_t colour_ptr = i_Brickmap[grid.getPtr].colourPtr;

                              uint bitcount = getSetBits(i_Brickmap[grid.getPtr], voxel_index);

                              i_Colours[colour_ptr + bitcount].r = (uint8_t)floor(colour.r * 255.);
                              i_Colours[colour_ptr + bitcount].g = (uint8_t)floor(colour.g * 255.);
                              i_Colours[colour_ptr + bitcount].b = (uint8_t)floor(colour.b * 255.);
                            }

                            break;
                          }
      case Type::ERASE: {
                          uint64_t new_value = i_Brickmap[grid.getPtr].data[voxel_index.y] & ~((uint64_t)1 << mask_index);
                          if (new_value == i_Brickmap[grid.getPtr].data[voxel_index.y]) {
                            break;
                          }

                          i_Brickmap[grid.getPtr].data[voxel_index.y] = new_value;
                          possiblyEmpty = (i_Brickmap[grid.getPtr].data[voxel_index.y] == 0);

                          uint bitcount = getSetBits(i_Brickmap[grid.getPtr], voxel_index);

                          eraseColour(bitcount, brick.colourPtr, oldcount);
                          break;
                        }
    }

    if (possiblyEmpty) {
      uint count = getAllSet(i_Brickmap[grid.getPtr]);

      if (count == 0) {
        uint brick_index = i_Brickgrid[brickgrid_index].getPtr;
        i_Brickgrid[brickgrid_index].isEmpty = true;

        // Mark colour as free
        uint64_t colour_ptr = i_Brickmap[grid.getPtr].colourPtr;
        i_Colours[colour_ptr].isUsed = false;
        if (i_Colours[colour_ptr].type == 0) {
          int size, stride;
          i_FreeColours.GetDimensions(size, stride);

          for (int i = 1; i < size; i++) {
            if (i_FreeColours[i] == 0) {
              i_FreeColours[i] = uint32_t(colour_ptr / 512) + 1;
              i_FreeColours[0] += 1;

              break;
            }
          }
        }

        int size, stride;
        i_FreeBricks.GetDimensions(size, stride);

        for (int i = 1; i < size; i++) {
          if (i_FreeBricks[i] == 0) {
            i_FreeBricks[i] = brick_index + 1;
            i_FreeBricks[0] += 1;

            break;
          }
        }

        tryMerge(colour_ptr);
      }
    }
  }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  uint3 voxel_index = push_constants.mod.voxel_index.rgb;

  run_shape<BrickmapModification>(voxel_index, push_constants.camera_facing, push_constants.mod);
}
