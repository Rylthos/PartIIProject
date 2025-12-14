#include "render_descriptor.slang"

func drawRect(in top_left : int2,
              in size : int2,
              in pixelcoords : int2,
              in target : float3,
              out colour : float3) -> void
{
  if (pixelcoords.x >= top_left.x && pixelcoords.x <= (top_left.x + size.x) &&
      pixelcoords.y >= top_left.y && pixelcoords.y <= (top_left.y + size.y)) {
    colour = target;
  }
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  int width, height;
  o_Image.GetDimensions(width, height);

  int2 pixelCoord    = int2(dispatchThreadID.xy);
  int2 window_size   = int2(width, height);
  float3 finalColour = float3(-1);

  drawRect(window_size / 2 - int2(1, 10), int2(2, 20), pixelCoord, float3(0.3), finalColour);
  drawRect(window_size / 2 - int2(10, 1), int2(20, 2), pixelCoord, float3(0.3), finalColour);

  if (any(finalColour >= float3(0))) {
    o_Image[pixelCoord] = float4(finalColour, 1);
  }
}
