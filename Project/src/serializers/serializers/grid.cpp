#include "grid.hpp"
#include "as_proto/general.pb.h"
#include "generators/grid.hpp"
#include "modification/diff.hpp"
#include "serializers/common.hpp"

#include "as_proto/grid.pb.h"

#include <iostream>

namespace Serializers {

std::ifstream loadGridFile(std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxgrid");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    return inputStream;
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(ASProto::Grid& grid)
{
    SerialInfo info = readHeader(grid.header());

    size_t voxelCount = grid.voxels_size();
    std::vector<Generators::GridVoxel> voxels;
    voxels.reserve(voxelCount);

    for (size_t i = 0; i < voxelCount; i++) {
        uint32_t rawVoxel = grid.voxels().at(i);
        glm::u8vec4 voxel = glm::u8vec4 {
            (rawVoxel >> 24) & 0xFF,
            (rawVoxel >> 16) & 0xFF,
            (rawVoxel >> 8) & 0xFF,
            (rawVoxel >> 0) & 0xFF,
        };

        voxels.push_back(Generators::GridVoxel {
            .visible = voxel.a > 0,
            .colour = {
                voxel.r / 255.f,
                voxel.g / 255.f,
                voxel.b / 255.f,
            },
            });
    }

    Modification::AnimationFrames animation;

    if (grid.has_animation()) {
        animation = readAnimation(grid.animation());
    }

    return std::make_tuple(info, voxels, animation);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(std::filesystem::path directory)
{
    std::ifstream inputStream = loadGridFile(directory);

    ASProto::Grid grid;
    grid.ParseFromIstream(&inputStream);

    return loadGrid(grid);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(const std::vector<uint8_t>& data)
{
    ASProto::Grid grid;
    grid.ParseFromArray(data.data(), data.size());

    return loadGrid(grid);
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

    ASProto::Grid gridProto;

    writeHeader(
        gridProto.mutable_header(), dimensions, generationInfo.voxelCount, generationInfo.nodes);

    for (const auto& voxel : grid) {
        uint32_t v = ((((uint32_t)(voxel.colour.r * 255.f)) & 0xFF) << 24)
            | ((((uint32_t)(voxel.colour.g * 255.f)) & 0xFF) << 16)
            | ((((uint32_t)(voxel.colour.b * 255.f)) & 0xFF) << 8)
            | ((((uint32_t)(voxel.visible)) & 0xFF) << 0);

        gridProto.mutable_voxels()->Add(v);
    }

    if (animation.size() != 0) {
        writeAnimation(gridProto.mutable_animation(), animation);
    }

    gridProto.SerializeToOstream(&outputStream);

    outputStream.close();
}
}
