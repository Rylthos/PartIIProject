#include "structures/brickmap.slang"

[[vk::binding(0, 0)]]
RWStructuredBuffer<Brickgrid> i_Brickgrid;

[[vk::binding(1, 0)]]
StructuredBuffer<Brickmap> i_Brickmap;

[[vk::binding(2, 0)]]
StructuredBuffer<uint8_t> i_Colours;

[[vk::binding(3, 0)]]
StructuredBuffer<uint32_t> i_Request;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint target = i_Request[dispatchThreadID.x + 1];
    i_Brickgrid[target].data &= ~0x3;
    i_Brickgrid[target].data |= 0x1;
}
