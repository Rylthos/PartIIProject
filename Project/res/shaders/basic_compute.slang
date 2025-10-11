layout(binding =  0, set = 0) RWTexture2D<float4> image;

[shader("compute")]
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  image[pixelCoord] = float4(0.0, 1.0, 1.0, 1.0);
}
