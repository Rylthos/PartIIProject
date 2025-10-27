struct OctreeNode {
  uint32_t data;

  property bool isLeaf {
    get { return (data & 0x80000000) != 0; }
  }
  property bool isSolid {
    get { return (data & 0x40000000) != 0; }
  }
  property uint32_t childMask { // Only lower 8 bits used
    get { return (data & 0x00FF0000) >> 8; }
  }
  property uint32_t offset {
    get { return (data & 0x0000FFFF); }
  }

  property float r {
    get { return ((data & 0xFF000000) >> 24) / 255.; }
  }
  property float g {
    get { return ((data & 0x00FF0000) >> 16) / 255.; }
  }
  property float b {
    get { return ((data & 0x0000FF00) >> 8) / 255.; }
  }

  property float3 colour {
    get { return float3(this.r, this.g, this.b); }
  }
};

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

[[vk::binding(0, 1)]]
StructuredBuffer<OctreeNode> i_Octree;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  int width, height;
  o_Image.GetDimensions(width, height);
  float aspect = float(width) / float(height);

  float2 uv = pixelCoord / float2(width, height);

  // o_Image[pixelCoord] = float4(uv, 0, 1);
  o_Image[pixelCoord] = float4(i_Octree[1].colour, 1);
}
