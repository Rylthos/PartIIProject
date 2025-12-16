#include "structures/brickmap.slang"

[[vk::binding(0, 0)]]
RWStructuredBuffer<Brickgrid> i_Brickgrid;

[[vk::binding(1, 0)]]
StructuredBuffer<Brickmap> i_Brickmap;

[[vk::binding(2, 0)]]
StructuredBuffer<uint8_t> i_Colours;

[[vk::binding(3, 0)]]
RWStructuredBuffer<uint32_t> i_Request;

[shader("compute")]
[numthreads(32, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  int size, stride;

  i_Request.GetDimensions(size, stride);

  if (dispatchThreadID.x < size - 1 && dispatchThreadID.x < i_Request[0]) {
    uint target = i_Request[dispatchThreadID.x + 1];
    i_Brickgrid[target].data &= ~0x3;
    i_Brickgrid[target].data |= 0x1;
  }
}
