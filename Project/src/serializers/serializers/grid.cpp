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
loadGrid(std::filesystem::path directory)
{
    std::ifstream inputStream = loadGridFile(directory);
    std::vector<uint8_t> data = vectorFromStream(inputStream);

    return loadGrid(data);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::GridVoxel>, Modification::AnimationFrames>>
loadGrid(const std::vector<uint8_t>& data)
{
    ASProto::Grid grid;
    grid.ParseFromArray(data.data(), data.size());

    ASProto::Header gridHeader = grid.header();

    SerialInfo info {
        .dimensions = glm::uvec3 { gridHeader.dimensions().x(), gridHeader.dimensions().y(),
                                  gridHeader.dimensions().z(), },
        .voxels = gridHeader.voxelcount(),
        .nodes = gridHeader.nodecount(),
    };

    printf("GRID: %d %d %d\n", info.dimensions.x, info.dimensions.y, info.dimensions.z);
    printf("Voxels: %ld\n", info.voxels);
    printf("Nodes: %ld\n", info.nodes);

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
        uint32_t frames = grid.animation().frames_size();
        animation.resize(frames);
        for (uint32_t frame = 0; frame < frames; frame++) {
            uint32_t diffs = grid.animation().frames().at(frame).diffs_size();
            for (uint32_t diff = 0; diff < diffs; diff++) {
                ASProto::AnimationDiff animDiff
                    = grid.animation().frames().at(frame).diffs().at(diff);

                glm::ivec3 index = {
                    animDiff.position().x(),
                    animDiff.position().y(),
                    animDiff.position().z(),
                };

                Modification::Type type = static_cast<Modification::Type>(animDiff.type());
                glm::vec3 colour = {
                    animDiff.colour().r(),
                    animDiff.colour().g(),
                    animDiff.colour().b(),
                };

                animation[frame].insert({
                    index, Modification::DiffType { type, colour }
                });
            }
        }
    }

    return std::make_tuple(info, voxels, animation);
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

    gridProto.mutable_header()->mutable_dimensions()->set_x(dimensions.x);
    gridProto.mutable_header()->mutable_dimensions()->set_y(dimensions.y);
    gridProto.mutable_header()->mutable_dimensions()->set_z(dimensions.z);

    gridProto.mutable_header()->set_voxelcount(generationInfo.voxelCount);
    gridProto.mutable_header()->set_nodecount(generationInfo.nodes);

    for (const auto& voxel : grid) {
        uint32_t v = ((((uint32_t)(voxel.colour.r * 255.f)) & 0xFF) << 24)
            | ((((uint32_t)(voxel.colour.g * 255.f)) & 0xFF) << 16)
            | ((((uint32_t)(voxel.colour.b * 255.f)) & 0xFF) << 8)
            | ((((uint32_t)(voxel.visible)) & 0xFF) << 0);

        gridProto.mutable_voxels()->Add(v);
    }

    for (const auto& frame : animation) {
        ASProto::AnimationFrames* frames = gridProto.mutable_animation()->mutable_frames()->Add();
        for (const auto& diff : frame) {
            ASProto::AnimationDiff* protoDiff = frames->mutable_diffs()->Add();
            protoDiff->set_type(static_cast<uint32_t>(diff.second.first));

            protoDiff->mutable_position()->set_x(diff.first.x);
            protoDiff->mutable_position()->set_y(diff.first.y);
            protoDiff->mutable_position()->set_z(diff.first.z);

            protoDiff->mutable_colour()->set_r(diff.second.second.r);
            protoDiff->mutable_colour()->set_g(diff.second.second.g);
            protoDiff->mutable_colour()->set_b(diff.second.second.b);
        }
    }

    gridProto.SerializeToOstream(&outputStream);

    outputStream.close();
}
}
