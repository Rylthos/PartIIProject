#include "general.slang"

struct PushConstants {
  uint3 dimensions;
  float3 camera_forward;
  ModStruct mod;
}

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture3D<float4> i_VoxelData;

struct TextureModification : IModification
{
  static func validIndex(in voxel_index : int3) -> bool
  {
    return all(voxel_index >= 0) && all(voxel_index < push_constants.dimensions);
  }

  static func setVoxel(in voxel_index : int3, in colour : float3, in type : Type) -> void
  {
    switch (type) {
      case Type::PLACE:
        i_VoxelData[voxel_index].a = 1;

        float3 colour = push_constants.mod.colour;
        i_VoxelData[voxel_index].rgb = colour;
        break;
      case Type::ERASE:
        i_VoxelData[voxel_index].a = 0;
        break;
    }
  }
}

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  uint3 voxel_index = push_constants.mod.voxel_index;

  run_shape<TextureModification>(voxel_index, push_constants.camera_forward, push_constants.mod);
}
