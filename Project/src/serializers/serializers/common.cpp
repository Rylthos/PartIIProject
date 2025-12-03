#include "common.hpp"

namespace Serializers {
void writeByte(uint8_t byte, std::ofstream& stream) { stream.put(byte); }

uint8_t readByte(std::ifstream& stream) { return stream.get(); }

void writeUint32(uint32_t value, std::ofstream& stream)
{
    for (size_t i = 0; i < 4; i++) {
        uint8_t byte = (value >> (8 * i)) & 0xFF;
        stream.put(byte);
    }
}

uint32_t readUint32(std::ifstream& stream)
{
    uint32_t output = 0;
    for (size_t i = 0; i < 4; i++) {
        uint8_t byte = readByte(stream);
        output |= ((uint32_t)byte) << (8 * i);
    }
    return output;
}

void writeUint64(uint64_t value, std::ofstream& stream)
{
    for (size_t i = 0; i < 8; i++) {
        uint8_t byte = (value >> (8 * i)) & 0xFF;
        stream.put(byte);
    }
}

uint64_t readUint64(std::ifstream& stream)
{

    uint64_t output = 0;
    for (size_t i = 0; i < 8; i++) {
        uint8_t byte = readByte(stream);
        output |= ((uint64_t)byte) << (8 * i);
    }
    return output;
}

void writeUvec3(glm::uvec3 vec, std::ofstream& stream)
{
    writeUint32(vec.x, stream);
    writeUint32(vec.y, stream);
    writeUint32(vec.z, stream);
}

glm::uvec3 readUvec3(std::ifstream& stream)
{
    glm::uvec3 vec;
    vec.x = readUint32(stream);
    vec.y = readUint32(stream);
    vec.z = readUint32(stream);
    return vec;
}

void writeU8Vec4(glm::u8vec4 vec, std::ofstream& stream)
{
    writeByte(vec.x, stream);
    writeByte(vec.y, stream);
    writeByte(vec.z, stream);
    writeByte(vec.w, stream);
}

glm::u8vec4 readU8Vec4(std::ifstream& stream)
{
    glm::u8vec4 vec;
    vec.x = readByte(stream);
    vec.y = readByte(stream);
    vec.z = readByte(stream);
    vec.w = readByte(stream);
    return vec;
}
}
