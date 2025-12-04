#include "brickmap.hpp"

#include "common.hpp"
#include "generators/brickmap.hpp"

#include <iostream>
#include <tuple>

namespace Serializers {

std::optional<std::tuple<SerialInfo, std::vector<Generators::BrickgridPtr>,
    std::vector<Generators::Brickmap>>>
loadBrickmap(std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxbrick");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    SerialInfo serialInfo;
    serialInfo.dimensions = Serializers::readUvec3(inputStream);
    serialInfo.voxels = Serializers::readUint64(inputStream);
    serialInfo.nodes = Serializers::readUint64(inputStream);

    glm::uvec3 brickgridSize = serialInfo.dimensions;

    std::vector<Generators::BrickgridPtr> brickgrid;
    size_t totalNodes = brickgridSize.x * brickgridSize.y * brickgridSize.z;
    brickgrid.reserve(totalNodes);

    std::vector<Generators::Brickmap> brickmaps;
    size_t nodes = serialInfo.nodes - totalNodes;
    brickmaps.reserve(nodes);

    for (size_t i = 0; i < totalNodes; i++) {
        brickgrid.push_back(Serializers::readUint32(inputStream));
    }

    for (size_t j = 0; j < nodes; j++) {
        Generators::Brickmap brickmap;
        brickmap.colourPtr = Serializers::readUint64(inputStream);
        for (int i = 0; i < 8; i++) {
            brickmap.occupancy[i] = Serializers::readUint64(inputStream);
        }
        uint64_t colours = Serializers::readUint64(inputStream);
        assert(colours <= 8 * 8 * 8 * 3);
        brickmap.colour.resize(colours);
        for (size_t i = 0; i < colours; i++) {
            brickmap.colour[i] = Serializers::readByte(inputStream);
        }
        brickmaps.push_back(brickmap);
    }

    return std::make_tuple(serialInfo, brickgrid, brickmaps);
}

void storeBrickmap(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::BrickgridPtr> brickgrid, std::vector<Generators::Brickmap> brickmaps,
    Generators::GenerationInfo generationInfo)
{
    std::filesystem::path target = output / name / (name + ".voxbrick");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    writeUvec3(dimensions, outputStream);
    writeUint64(generationInfo.voxelCount, outputStream);
    writeUint64(generationInfo.nodes, outputStream);

    for (const uint32_t& ptr : brickgrid) {
        Serializers::writeUint32(ptr, outputStream);
    }

    for (const Generators::Brickmap& brickmap : brickmaps) {
        Serializers::writeUint64(brickmap.colourPtr, outputStream);
        for (uint8_t i = 0; i < 8; i++) {
            Serializers::writeUint64(brickmap.occupancy[i], outputStream);
        }
        Serializers::writeUint64(brickmap.colour.size(), outputStream);
        for (size_t i = 0; i < brickmap.colour.size(); i++) {
            Serializers::writeByte(brickmap.colour[i], outputStream);
        }
    }

    outputStream.close();
}
}
