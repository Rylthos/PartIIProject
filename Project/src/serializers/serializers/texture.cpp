#include "texture.hpp"

#include "common.hpp"
#include "generators/texture.hpp"

#include <iostream>

namespace Serializers {

std::optional<std::tuple<SerialInfo, std::vector<Generators::TextureVoxel>>> loadTexture(
    std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxtexture");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    SerialInfo serialInfo;
    serialInfo.dimensions = Serializers::readUvec3(inputStream);
    serialInfo.voxels = Serializers::readUint64(inputStream);
    serialInfo.nodes = Serializers::readUint64(inputStream);

    std::vector<Generators::TextureVoxel> nodes;
    nodes.reserve(serialInfo.nodes);

    while (!inputStream.eof()) {
        glm::u8vec4 data = Serializers::readU8Vec4(inputStream);

        nodes.push_back(data);
    }
    return std::make_pair(serialInfo, nodes);
}

void storeTexture(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::TextureVoxel> voxels, Generators::GenerationInfo generationInfo)
{
    std::filesystem::path target = output / name / (name + ".voxtexture");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    writeUvec3(dimensions, outputStream);
    writeUint64(generationInfo.voxelCount, outputStream);
    writeUint64(generationInfo.nodes, outputStream);

    for (const auto& node : voxels) {
        writeU8Vec4(node, outputStream);
    }

    outputStream.close();
}
}
