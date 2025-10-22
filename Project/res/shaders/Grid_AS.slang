
[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  o_Image[pixelCoord] = float4(1.0, 0.0, 0.0, 1.0);
}
