#include "texture.hpp"

#include "common.hpp"
#include "generators/texture.hpp"
#include "modification/diff.hpp"

#include "as_proto/texture.pb.h"

namespace Serializers {

std::ifstream loadTextureFile(std::filesystem::path directory)
{

    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxtexture");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    return inputStream;
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>, Modification::AnimationFrames>>
loadTexture(ASProto::Texture& texture)
{
    SerialInfo serialInfo = readHeader(texture.header());

    size_t voxelCount = texture.voxels_size();
    std::vector<Generators::TextureVoxel> voxels;
    voxels.reserve(voxelCount);

    for (size_t i = 0; i < voxelCount; i++) {
        uint32_t rawVoxel = texture.voxels().at(i);

        glm::u8vec4 voxel = glm::u8vec4 {
            (rawVoxel >> 24) & 0xFF,
            (rawVoxel >> 16) & 0xFF,
            (rawVoxel >> 8) & 0xFF,
            (rawVoxel >> 0) & 0xFF,
        };

        voxels.push_back(voxel);
    }
    Modification::AnimationFrames animation;
    if (texture.has_animation()) {
        animation = readAnimation(texture.animation());
    }

    return std::make_tuple(serialInfo, voxels, animation);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>, Modification::AnimationFrames>>
loadTexture(std::filesystem::path directory)
{
    std::ifstream inputStream = loadTextureFile(directory);

    ASProto::Texture texture;
    texture.ParseFromIstream(&inputStream);

    return loadTexture(texture);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>, Modification::AnimationFrames>>
loadTexture(const std::vector<uint8_t>& data)
{
    ASProto::Texture texture;
    texture.ParseFromArray(data.data(), data.size());

    return loadTexture(texture);
}

void storeTexture(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::TextureVoxel> voxels, Generators::GenerationInfo generationInfo,
    const Modification::AnimationFrames& animation)
{
    std::filesystem::path target = output / name / (name + ".voxtexture");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    ASProto::Texture textureProto;

    writeHeader(
        textureProto.mutable_header(), dimensions, generationInfo.voxelCount, generationInfo.nodes);

    for (const auto& voxel : voxels) {
        uint32_t v = (((uint32_t)(voxel.r)) << 24) | (((uint32_t)(voxel.g)) << 16)
            | (((uint32_t)(voxel.b)) << 8) | (((uint32_t)(voxel.a)) << 0);

        textureProto.add_voxels(v);
    }

    if (animation.size() != 0) {
        writeAnimation(textureProto.mutable_animation(), animation);
    }

    textureProto.SerializeToOstream(&outputStream);

    outputStream.close();
}
}
