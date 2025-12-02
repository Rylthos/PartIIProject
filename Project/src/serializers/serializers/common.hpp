#pragma once

#include <fstream>

#include <cstdint>

#include <glm/glm.hpp>

namespace Serializers {
void writeByte(uint8_t byte, std::ofstream& stream);
uint8_t readByte(std::ifstream& stream);

void writeUint32(uint32_t byte, std::ofstream& stream);
uint32_t readUint32(std::ifstream& stream);

void writeUvec3(glm::uvec3 vec, std::ofstream& stream);
glm::uvec3 readUvec3(std::ifstream& stream);

void writeU8Vec4(glm::u8vec4 vec, std::ofstream& stream);
glm::u8vec4 readU8Vec4(std::ifstream& stream);
};
