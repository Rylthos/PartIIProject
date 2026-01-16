#include "common.hpp"
#include "modification/diff.hpp"
#include "modification/mod_type.hpp"

namespace Serializers {
void writeByte(uint8_t byte, std::ofstream& stream) { stream.put(byte); }

uint8_t readByte(std::istream& stream) { return stream.get(); }

void writeUint32(uint32_t value, std::ofstream& stream)
{
    for (size_t i = 0; i < 4; i++) {
        uint8_t byte = (value >> (8 * i)) & 0xFF;
        stream.put(byte);
    }
}

uint32_t readUint32(std::istream& stream)
{
    uint32_t output = 0;
    for (size_t i = 0; i < 4; i++) {
        uint8_t byte = readByte(stream);
        output |= ((uint32_t)byte) << (8 * i);
    }
    return output;
}

void writeFloat(float value, std::ofstream& stream)
{
    uint32_t converted = std::bit_cast<uint32_t>(value);

    writeUint32(converted, stream);
}

float readFloat(std::istream& stream)
{
    uint32_t converted = readUint32(stream);
    return std::bit_cast<float>(converted);
}

void writeUint64(uint64_t value, std::ofstream& stream)
{
    for (size_t i = 0; i < 8; i++) {
        uint8_t byte = (value >> (8 * i)) & 0xFF;
        stream.put(byte);
    }
}

uint64_t readUint64(std::istream& stream)
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

glm::uvec3 readUvec3(std::istream& stream)
{
    glm::uvec3 vec;
    vec.x = readUint32(stream);
    vec.y = readUint32(stream);
    vec.z = readUint32(stream);
    return vec;
}

void writeVec3(glm::vec3 vec, std::ofstream& stream)
{
    writeFloat(vec.x, stream);
    writeFloat(vec.y, stream);
    writeFloat(vec.z, stream);
}

glm::vec3 readVec3(std::istream& stream)
{
    glm::vec3 vec;

    vec.x = readFloat(stream);
    vec.y = readFloat(stream);
    vec.z = readFloat(stream);

    return vec;
}

void writeU8Vec4(glm::u8vec4 vec, std::ofstream& stream)
{
    writeByte(vec.x, stream);
    writeByte(vec.y, stream);
    writeByte(vec.z, stream);
    writeByte(vec.w, stream);
}

glm::u8vec4 readU8Vec4(std::istream& stream)
{
    glm::u8vec4 vec;
    vec.x = readByte(stream);
    vec.y = readByte(stream);
    vec.z = readByte(stream);
    vec.w = readByte(stream);
    return vec;
}

void writeDiff(Modification::DiffType diff, std::ofstream& stream)
{
    writeUint32(static_cast<uint32_t>(diff.first), stream);
    writeVec3(diff.second, stream);
}

Modification::DiffType readDiff(std::istream& stream)
{
    Modification::Type type = static_cast<Modification::Type>(readUint32(stream));
    glm::vec3 colour = readVec3(stream);

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

Modification::AnimationFrames readAnimationFrames(std::istream& stream)
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

}
