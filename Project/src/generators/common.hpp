#pragma once

#include <cstdint>

namespace Generators {
struct GenerationInfo {
    float completionPercent;
    float generationTime;

    uint64_t voxelCount;
    uint64_t nodes;
};
}
