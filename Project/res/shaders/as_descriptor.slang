#pragma once

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Positions;

[[vk::binding(1, 0)]]
RWTexture2D<float4> o_Colours;

[[vk::binding(2, 0)]]
RWTexture2D<float4> o_Normals;

[[vk::binding(3, 0)]]
RWTexture2D<float> o_Depth;

[[vk::binding(4, 0)]]
RWTexture2D<float4> i_RayDirectionImage;
