#include "general.slang"

struct PushConstants {
  uint3 dimensions;
  uint modCount;
  float3 camera_forward;
  uint64_t modBuffer;
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

        i_VoxelData[voxel_index].rgb = colour;
        break;
      case Type::REPLACE:
        i_VoxelData[voxel_index].a = 1;

        if (i_VoxelData[voxel_index].a != 0) {
          i_VoxelData[voxel_index].rgb = colour;
        }
        break;

      case Type::ERASE:
        i_VoxelData[voxel_index].a = 0;
        break;
    }
  }
}

[shader("compute")]
[numthreads(32, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  ModStruct* mods = (ModStruct*)push_constants.modBuffer;

  if (dispatchThreadID.x >= push_constants.modCount)
    return;

  ModStruct mod = mods[dispatchThreadID.x];

  uint3 voxel_index = mod.voxel_index.rgb;

  run_shape<TextureModification>(voxel_index, push_constants.camera_forward, mod);
}
