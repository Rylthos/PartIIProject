#pragma once

#include "modification/diff.hpp"
#include <fstream>

#include <cstdint>

#include <glm/glm.hpp>

namespace Serializers {
struct SerialInfo {
    glm::uvec3 dimensions;
    uint64_t voxels;
    uint64_t nodes;
};

void writeByte(uint8_t byte, std::ofstream& stream);

uint8_t readByte(std::istream& stream);

void writeUint32(uint32_t value, std::ofstream& stream);

uint32_t readUint32(std::istream& stream);

void writeFloat(float value, std::ofstream& stream);

float readFloat(std::istream& stream);

void writeUint64(uint64_t value, std::ofstream& stream);

uint64_t readUint64(std::istream& stream);

void writeUvec3(glm::uvec3 vec, std::ofstream& stream);

glm::uvec3 readUvec3(std::istream& stream);

void writeVec3(glm::vec3 vec, std::ofstream& stream);

glm::vec3 readVec3(std::istream& stream);

void writeU8Vec4(glm::u8vec4 vec, std::ofstream& stream);

glm::u8vec4 readU8Vec4(std::istream& stream);

void writeDiff(Modification::DiffType diff, std::ofstream& stream);

Modification::DiffType readDiff(std::istream& stream);

void writeAnimationFrames(const Modification::AnimationFrames& animation, std::ofstream& stream);

Modification::AnimationFrames readAnimationFrames(std::istream& stream);
};
