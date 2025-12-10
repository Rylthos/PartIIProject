#include "general.slang"

struct PushConstants {
  uint3 dimensions;
  ModStruct mod;
}

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture3D<float4> i_VoxelData;

struct TextureModification : IModification {
  static func validIndex(in voxel_index: int3) -> bool {
    return all(voxel_index >= 0) && all(voxel_index < push_constants.dimensions);
  }

  static func setVoxel(in voxel_index: int3, in colour: float3, in place: bool) -> void {
    if (place) {
      i_VoxelData[voxel_index].a = 1;

      float3 colour = push_constants.mod.colour;
      i_VoxelData[voxel_index].rgb = colour;
    } else {
      i_VoxelData[voxel_index].a = 0;
    }
  }
}


[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  uint3 voxel_index = push_constants.mod.voxel_index;

  if (!TextureModification::validIndex(voxel_index))
    return;

  bool place = push_constants.mod.place == 1;
  if (push_constants.mod.shape == SHAPE_VOXEL) {
    TextureModification::setVoxel(voxel_index, push_constants.mod.colour, place);
  } else if (push_constants.mod.shape == SHAPE_SPHERE) {
    sphere<TextureModification>(voxel_index, int(push_constants.mod.additional.x), push_constants.mod.colour, place);
  }
}
