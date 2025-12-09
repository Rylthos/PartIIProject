#pragma once

[[vk::binding(0, 1)]]
StructuredBuffer<uint32_t> i_Occupancy;

[[vk::binding(1, 1)]]
StructuredBuffer<float> i_Colour;
