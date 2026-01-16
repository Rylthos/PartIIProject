#include "grid.hpp"
#include "generators/grid.hpp"
#include "modification/diff.hpp"
#include "serializers/common.hpp"

#include <iostream>

namespace Serializers {

std::ifstream loadGridFile(std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxgrid");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in | std::ios::ate);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    return inputStream;
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(std::filesystem::path directory)
{
    std::ifstream inputStream = loadGridFile(directory);

    std::vector<uint8_t> data = vectorFromStream(inputStream);

    return loadGrid(data);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(std::vector<uint8_t> data)
{
    SerialInfo serialInfo;

    std::istringstream inputStream(std::string(data.begin(), data.end()));

    serialInfo.dimensions = Serializers::readUvec3(inputStream);
    serialInfo.voxels = Serializers::readUint64(inputStream);
    serialInfo.nodes = Serializers::readUint64(inputStream);
    size_t voxelCount = Serializers::readUint64(inputStream);

    std::vector<Generators::GridVoxel> voxels;
    voxels.reserve(voxelCount);

    for (size_t i = 0; i < voxelCount; i++) {
        glm::u8vec4 voxel = Serializers::readU8Vec4(inputStream);

        voxels.push_back(Generators::GridVoxel {
                .visible = voxel.a > 0,
                .colour = {
                    voxel.r / 255.f,
                    voxel.g / 255.f,
                    voxel.b / 255.f,
                },
        });
    }

    Modification::AnimationFrames animation = readAnimationFrames(inputStream);

    return std::make_tuple(serialInfo, voxels, animation);
}

void storeGrid(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::GridVoxel> grid, Generators::GenerationInfo generationInfo,
    const Modification::AnimationFrames& animation)
{
    std::filesystem::path target = output / name / (name + ".voxgrid");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    writeUvec3(dimensions, outputStream);
    writeUint64(generationInfo.voxelCount, outputStream);
    writeUint64(generationInfo.nodes, outputStream);
    writeUint64(grid.size(), outputStream);

    for (const auto& voxel : grid) {
        glm::u8vec4 v = {
            voxel.colour.r * 255.f,
            voxel.colour.g * 255.f,
            voxel.colour.b * 255.f,
            voxel.visible,
        };
        Serializers::writeU8Vec4(v, outputStream);
    }

    writeAnimationFrames(animation, outputStream);

    outputStream.close();
}
}
