#include "grid.hpp"
#include "generators/grid.hpp"
#include "serializers/common.hpp"

#include <iostream>

namespace Serializers {

std::optional<std::tuple<SerialInfo, std::vector<Generators::GridVoxel>>> loadGrid(
    std::filesystem::path directory)
{
    if (directory.has_filename()) {
        LOG_ERROR("Expected a directory not a file\n");
        return {};
    }

    std::string foldername = directory.parent_path().filename();

    std::filesystem::path file = directory / (foldername + ".voxgrid");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    SerialInfo serialInfo;

    serialInfo.dimensions = Serializers::readUvec3(inputStream);
    serialInfo.voxels = Serializers::readUint64(inputStream);
    serialInfo.nodes = Serializers::readUint64(inputStream);

    std::vector<Generators::GridVoxel> voxels;
    voxels.reserve(serialInfo.voxels);

    while (!inputStream.eof()) {
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
    return std::make_pair(serialInfo, voxels);
}

void storeGrid(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::GridVoxel> grid, Generators::GenerationInfo generationInfo)
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

    for (const auto& voxel : grid) {
        glm::u8vec4 v = {
            voxel.colour.r * 255.f,
            voxel.colour.g * 255.f,
            voxel.colour.b * 255.f,
            voxel.visible,
        };
        Serializers::writeU8Vec4(v, outputStream);
    }

    outputStream.close();
}
}
