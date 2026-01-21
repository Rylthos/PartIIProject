#include "as_descriptor.slang"
#include "render_descriptor.slang"

#include "lighting.slang"

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  int width, height;
  o_Image.GetDimensions(width, height);

  int2 pixelCoord    = int2(dispatchThreadID.xy);

  if (o_Colours[pixelCoord].a > 0) {
    o_Image[pixelCoord] = float4(o_Colours[pixelCoord].xyz, 1.);
  } else {
    o_Image[pixelCoord] = missColour();
  }
}
