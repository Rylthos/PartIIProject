#include "gBuffer_descriptor.slang"
#include "render_descriptor.slang"

#include "lighting.slang"

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
  int width, height;
  o_Image.GetDimensions(width, height);

  int2 pixelCoord = int2(dispatchThreadID.xy);

  int type = int(o_Colours[pixelCoord].a);

  switch (type) {
    case 0: // Normal
      if (o_Depth[pixelCoord] > 0) {
        float3 pos = o_Positions[pixelCoord].xyz;
        float3 normal = o_Normals[pixelCoord].xyz;
        float3 colour = o_Colours[pixelCoord].rgb;
        o_Image[pixelCoord] = calculateColour(pos, normal, colour, false);
      } else {
        o_Image[pixelCoord] = missColour();
      }
      break;
    case 1: // Heatmap
      o_Image[pixelCoord] = float4(o_Colours[pixelCoord].rgb, 1.);
      break;
    case 2: // Cycles
      o_Image[pixelCoord] = float4(o_Colours[pixelCoord].rgb, 1.);
      break;
    default:
    break;
  }
}
