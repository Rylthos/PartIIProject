#include "ray.slang"
#include "push_constants.slang"
#include "default_defines.slang"

#ifndef MAX_DEPTH
#define MAX_DEPTH 10
#endif

struct OctreeNode {
  uint32_t data;

  __init(uint32_t data) {
    this.data = data;
  }

  // Flags
  property bool isSolid {
    get { return (data & 0x01000000) != 0; }
  }

  // Node properties
  property uint32_t childMask { // Only lower 8 bits used
    get { return (data & 0x00FF0000) >> 8; }
  }
  property uint32_t offset {
    get { return (data & 0x0000FFFF); }
  }

  // Lead properties
  property float r {
    get { return ((data & 0x00FF0000) >> 16) / 255.; }
  }
  property float g {
    get { return ((data & 0x0000FF00) >> 8) / 255.; }
  }
  property float b {
    get { return ((data & 0x000000FF) >> 0) / 255.; }
  }

  property float3 colour {
    get { return float3(this.r, this.g, this.b); }
  }
};

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

[[vk::binding(0, 1)]]
StructuredBuffer<OctreeNode> i_Octree;

func traverse(in ray : Ray, in startingT : float, min_bound : float3, max_bound : float3) -> HitRecord {
  HitRecord hit;
  if (startingT < 0.) {
    return hit;
  }

  float3 entry_pos = ray.calculate(startingT + 0.00001);

  uint current_index = 0;
  uint stack[MAX_DEPTH];
  stack[0] = 0;

  int current_depth = 1;
  float default_scale = 1.0f;

  for (int i = 0; i < STEP_LIMIT; i++) {
    float current_scale = default_scale / (1 << current_depth);

    OctreeNode node = new OctreeNode(i_Octree[stack[current_index]]);

    if (node.isSolid) {
      hit.hit = true;
      hit.colour = node.colour;
      return hit;
    }

    uint32_t childMask = node.childMask;
    uint32_t offset = node.offset;

    // Calculate hit voxel
    // Update ray position if required
    // Descend of ascend based on voxel position
  }

  return hit;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  int width, height;
  o_Image.GetDimensions(width, height);
  float aspect = float(width) / float(height);

  float2 uv = pixelCoord / float2(width, height);

  Ray ray = Ray(aspect,
                uv,
                push_constants.camera_position,
                push_constants.camera_front,
                push_constants.camera_right,
                push_constants.camera_up
  );

  const float3 min_bound = float3(0);
  const float3 max_bound = float3(1);

  float startingT = ray.aabb(min_bound, max_bound);
  HitRecord hit = traverse(ray, startingT, min_bound, max_bound);

  if (hit.hit) {
    o_Image[pixelCoord] = float4(hit.colour, 1.);
  } else {
    o_Image[pixelCoord] = float4(float3(0.2), 1);
  }
}
