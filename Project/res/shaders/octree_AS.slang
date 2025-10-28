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
    get { return (data & 0x00FF0000) >> 16; }
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

struct StackMember {
  uint parent;
  float3 minBound;
  float tMax;
}

func traverse(in ray : Ray) -> HitRecord {
  float3 minBound = float3(0);
  float3 maxBound = float3(1);

  const float3 dimensions = maxBound - minBound;

  HitRecord hit;
  float tMin, tMax;
  if (!ray.aabb(minBound, maxBound, tMin, tMax, 0.0000001, 100000.0)) {
    return hit;
  }

  float3 position = ray.calculate(tMin + 0.00001);

  int current_index = -1;
  StackMember stack[MAX_DEPTH];

  float current_scale = 0.5;
  uint parent = 0;

  OctreeNode node;
  float t = tMin;

  for (int i = 0; i < STEP_LIMIT; i++) {
    node = new OctreeNode(i_Octree[parent]);

    if (node.isSolid) {
      hit.hit = true;
      hit.colour = node.colour;

      return hit;
    }

    if (t >= tMax) {
      // Ascend
      if (current_index == -1) break;

      StackMember member = stack[current_index];
      current_index--;

      tMax = member.tMax;
      parent = member.parent;
      minBound = member.minBound;

      node = new OctreeNode(i_Octree[parent]);
      current_scale *= 2;

      continue;
    }

    float3 center = minBound + current_scale * dimensions;
    float3 boundOffset = float3(0);
    int octantMask = 0;

    if (position.x > center.x || (position.x == center.x && ray.direction.x > 0)) {
      octantMask ^= 1;
      boundOffset.x = dimensions.x;
    }

    if (position.z > center.z || (position.z == center.z && ray.direction.z > 0)) {
      octantMask ^= 2;
      boundOffset.z = dimensions.z;
    }

    if (position.y > center.y || (position.y == center.y && ray.direction.y > 0)) {
      octantMask ^= 4;
      boundOffset.y = dimensions.y;
    }

    bool childMask = ((node.childMask >> octantMask) & 1) == 1;

    if (childMask) { // Child exists and has content
      // Descend into child
      if (node.offset == 0) break;

      StackMember stackMember;
      stackMember.parent = parent;
      stackMember.tMax = tMax;
      stackMember.minBound = minBound;

      stack[current_index + 1] = stackMember;
      current_index++;

      uint offset = countbits(node.childMask >> (octantMask + 1));
      parent = parent + node.offset + offset;

      minBound += boundOffset * current_scale;
      maxBound = minBound + current_scale * dimensions;

      if (!ray.aabb(minBound, maxBound, tMin, tMax, tMin, tMax))
        break;

      current_scale *= 0.5;

      continue;
    } else { // Child is empty
      // Traverse through air
      float3 octantMinBound = minBound + boundOffset * current_scale;
      float3 octantMaxBound = octantMinBound + current_scale * dimensions;

      float t0;
      if (!ray.aabb(octantMinBound, octantMaxBound, t0, t, tMin, tMax))
        break;

      t += 0.0000001;
      position = ray.calculate(t);

      continue;
    }
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

  HitRecord hit = traverse(ray);

  if (hit.hit) {
    o_Image[pixelCoord] = float4(hit.colour, 1.);
  } else {
    o_Image[pixelCoord] = float4(float3(0.2), 1);
  }
}
