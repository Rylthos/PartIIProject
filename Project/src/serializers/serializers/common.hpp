#pragma once

#include "modification/diff.hpp"

#include "as_proto/general.pb.h"

#include <glm/glm.hpp>

namespace Serializers {
struct SerialInfo {
    glm::uvec3 dimensions;
    uint64_t voxels;
    uint64_t nodes;
};

std::vector<uint8_t> vectorFromStream(std::istream& stream);

void writeHeader(
    ASProto::Header* header, glm::uvec3 dimensions, size_t voxelCount, size_t nodeCount);

SerialInfo readHeader(const ASProto::Header& header);

void writeDiff(ASProto::AnimationDiff* diff, glm::ivec3 position, Modification::DiffType diffType);

std::pair<glm::ivec3, Modification::DiffType> readDiff(const ASProto::AnimationDiff& diff);

void writeAnimation(ASProto::Animation* animation, const Modification::AnimationFrames& frames);

Modification::AnimationFrames readAnimation(const ASProto::Animation& animation);
};
