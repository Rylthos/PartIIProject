#include "test_include.slang"

struct PushConstants {
  float2 mousePos;
  bool renderFull;
};

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 0)]]
RWTexture2D<float4> image;

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int2 pixelCoord = int2(dispatchThreadID.xy);

  float3 colour = float3(0);
  float2 lerpAmounts = push_constants.mousePos;

  if (push_constants.renderFull) {
    int width, height;
    image.GetDimensions(width, height);

    lerpAmounts = float2(pixelCoord) / float2(width, height);
  }

  float3 T = lerp(colourTL, colourTR, float3(lerpAmounts.x));
  float3 B = lerp(colourBL, colourBR, float3(lerpAmounts.x));

  colour = lerp(T, B, float3(lerpAmounts.y));

  image[pixelCoord] = float4(colour, 1.0);
}
