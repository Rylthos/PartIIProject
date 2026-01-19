#pragma once

#include <cstdint>

namespace Generators {
struct GenerationInfo {
    float completionPercent = 0.f;
    float generationTime = 0.f;

    uint64_t voxelCount = 0;
    uint64_t nodes = 0;
};
}
