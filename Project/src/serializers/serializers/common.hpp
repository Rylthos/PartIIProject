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

uint8_t readByte(std::ifstream& stream);
uint8_t readByte(const std::vector<uint8_t>& data, uint32_t& index);

void writeUint32(uint32_t value, std::ofstream& stream);

uint32_t readUint32(std::ifstream& stream);
uint32_t readUint32(const std::vector<uint8_t>& data, uint32_t& index);

void writeFloat(float value, std::ofstream& stream);

float readFloat(std::ifstream& stream);
float readFloat(const std::vector<uint8_t>& data, uint32_t& index);

void writeUint64(uint64_t value, std::ofstream& stream);

uint64_t readUint64(std::ifstream& stream);
uint64_t readUint64(const std::vector<uint8_t>& data, uint32_t& index);

void writeUvec3(glm::uvec3 vec, std::ofstream& stream);

glm::uvec3 readUvec3(std::ifstream& stream);
glm::uvec3 readUvec3(const std::vector<uint8_t>& data, uint32_t& index);

void writeVec3(glm::vec3 vec, std::ofstream& stream);

glm::vec3 readVec3(std::ifstream& stream);
glm::vec3 readVec3(const std::vector<uint8_t>& data, uint32_t& index);

void writeU8Vec4(glm::u8vec4 vec, std::ofstream& stream);

glm::u8vec4 readU8Vec4(std::ifstream& stream);
glm::u8vec4 readU8Vec4(const std::vector<uint8_t>& data, uint32_t& index);

void writeDiff(Modification::DiffType diff, std::ofstream& stream);

Modification::DiffType readDiff(std::ifstream& stream);
Modification::DiffType readDiff(const std::vector<uint8_t>& data, uint32_t& index);

void writeAnimationFrames(const Modification::AnimationFrames& animation, std::ofstream& stream);

Modification::AnimationFrames readAnimationFrames(std::ifstream& stream);
Modification::AnimationFrames readAnimationFrames(
    const std::vector<uint8_t>& data, uint32_t& index);
};
