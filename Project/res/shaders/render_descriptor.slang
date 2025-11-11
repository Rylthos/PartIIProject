#pragma once

[[vk::binding(0, 0)]]
RWTexture2D<float4> o_Image;

[[vk::binding(1, 0)]]
RWTexture2D<float4> i_RayDirectionImage;
