#include "common.hpp"
#include "modification/diff.hpp"
#include "modification/mod_type.hpp"

namespace Serializers {
void writeByte(uint8_t byte, std::ofstream& stream) { stream.put(byte); }

uint8_t readByte(std::ifstream& stream) { return stream.get(); }

uint8_t readByte(const std::vector<uint8_t>& data, uint32_t& index) { return data[index++]; }

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

uint32_t readUint32(const std::vector<uint8_t>& data, uint32_t& index)
{
    uint32_t output = 0;
    for (size_t i = 0; i < 4; i++) {
        uint8_t byte = readByte(data, index);
        output |= ((uint32_t)byte) << (8 * i);
    }
    return output;
}

void writeFloat(float value, std::ofstream& stream)
{
    uint32_t converted = std::bit_cast<uint32_t>(value);

    writeUint32(converted, stream);
}

float readFloat(std::ifstream& stream)
{
    uint32_t converted = readUint32(stream);
    return std::bit_cast<float>(converted);
}

float readFloat(const std::vector<uint8_t>& data, uint32_t& index)
{
    uint32_t converted = readUint32(data, index);
    return std::bit_cast<float>(converted);
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

uint64_t readUint64(const std::vector<uint8_t>& data, uint32_t& index)
{
    uint64_t output = 0;
    for (size_t i = 0; i < 8; i++) {
        uint8_t byte = readByte(data, index);
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

glm::uvec3 readUvec3(const std::vector<uint8_t>& data, uint32_t& index)
{
    glm::uvec3 vec;
    vec.x = readUint32(data, index);
    vec.y = readUint32(data, index);
    vec.z = readUint32(data, index);
    return vec;
}

void writeVec3(glm::vec3 vec, std::ofstream& stream)
{
    writeFloat(vec.x, stream);
    writeFloat(vec.y, stream);
    writeFloat(vec.z, stream);
}

glm::vec3 readVec3(std::ifstream& stream)
{
    glm::vec3 vec;

    vec.x = readFloat(stream);
    vec.y = readFloat(stream);
    vec.z = readFloat(stream);

    return vec;
}

glm::vec3 readVec3(const std::vector<uint8_t>& data, uint32_t& index)
{
    glm::vec3 vec;

    vec.x = readFloat(data, index);
    vec.y = readFloat(data, index);
    vec.z = readFloat(data, index);

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

glm::u8vec4 readU8Vec4(const std::vector<uint8_t>& data, uint32_t& index)
{
    glm::u8vec4 vec;
    vec.x = readByte(data, index);
    vec.y = readByte(data, index);
    vec.z = readByte(data, index);
    vec.w = readByte(data, index);
    return vec;
}

void writeDiff(Modification::DiffType diff, std::ofstream& stream)
{
    writeUint32(static_cast<uint32_t>(diff.first), stream);
    writeVec3(diff.second, stream);
}

Modification::DiffType readDiff(std::ifstream& stream)
{
    Modification::Type type = static_cast<Modification::Type>(readUint32(stream));
    glm::vec3 colour = readVec3(stream);

    return { type, colour };
}

Modification::DiffType readDiff(const std::vector<uint8_t>& data, uint32_t& index)
{
    Modification::Type type = static_cast<Modification::Type>(readUint32(data, index));
    glm::vec3 colour = readVec3(data, index);

    return { type, colour };
}

void writeAnimationFrames(const Modification::AnimationFrames& animation, std::ofstream& stream)
{
    writeUint64(animation.size(), stream);

    for (const auto& frame : animation) {
        writeUint64(frame.size(), stream);

        for (const auto& change : frame) {
            writeUvec3(change.first, stream);

            writeDiff(change.second, stream);
        }
    }
}

Modification::AnimationFrames readAnimationFrames(std::ifstream& stream)
{
    Modification::AnimationFrames animation;

    size_t animationFrames = readUint64(stream);

    animation.resize(animationFrames);

    for (size_t frame = 0; frame < animationFrames; frame++) {
        size_t changes = readUint64(stream);

        for (size_t change = 0; change < changes; change++) {
            glm::ivec3 index = readUvec3(stream);

            Modification::DiffType diff = readDiff(stream);

            animation[frame].insert({ index, diff });
        }
    }

    return animation;
}

Modification::AnimationFrames readAnimationFrames(const std::vector<uint8_t>& data, uint32_t& index)
{
    Modification::AnimationFrames animation;

    size_t animationFrames = readUint64(data, index);

    animation.resize(animationFrames);

    for (size_t frame = 0; frame < animationFrames; frame++) {
        size_t changes = readUint64(data, index);

        for (size_t change = 0; change < changes; change++) {
            glm::ivec3 voxelIndex = readUvec3(data, index);

            Modification::DiffType diff = readDiff(data, index);

            animation[frame].insert({ voxelIndex, diff });
        }
    }

    return animation;
}

}
