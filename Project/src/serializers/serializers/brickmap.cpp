#include "brickmap.hpp"

#include "common.hpp"
#include "generators/brickmap.hpp"
#include "modification/diff.hpp"

#include "as_proto/brickmap.pb.h"

#include <fstream>
#include <tuple>

namespace Serializers {

std::ifstream loadBrickmapFile(std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxbrick");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    return inputStream;
}
std::optional<
    std::tuple<SerialInfo, std::vector<Generators::BrickgridPtr>, std::vector<Generators::Brickmap>,
        std::vector<Generators::BrickmapColour>, Modification::AnimationFrames>>
loadBrickmap(ASProto::Brickmap& brickmap)
{
    SerialInfo serialInfo = readHeader(brickmap.header());

    size_t brickgridSize = brickmap.grid().pointers_size();
    std::vector<Generators::BrickgridPtr> brickgrid;
    brickgrid.reserve(brickgridSize);

    size_t brickmapSize = brickmap.bricks_size();
    std::vector<Generators::Brickmap> brickmaps;
    brickmaps.reserve(brickmapSize);

    size_t coloursSize = brickmap.colours_size();
    std::vector<Generators::BrickmapColour> colours;
    brickmaps.reserve(coloursSize);

    for (size_t i = 0; i < brickgridSize; i++) {
        brickgrid.push_back(brickmap.grid().pointers().at(i));
    }

    for (size_t i = 0; i < brickmapSize; i++) {
        ASProto::Brick protoBrick = brickmap.bricks().at(i);

        Generators::Brickmap brickmap;
        brickmap.colourPtr = protoBrick.colour_ptr();

        for (int j = 0; j < 8; j++) {
            brickmap.occupancy[j] = protoBrick.occupancy().at(j);
        }
        brickmaps.push_back(brickmap);
    }

    for (size_t i = 0; i < coloursSize; i++) {
        ASProto::BrickColour protoColour = brickmap.colours().at(i);

        Generators::BrickmapColour colour;

        colour.data = (protoColour.data() >> 24) & 0xFF;
        colour.r = (protoColour.data() >> 16) & 0xFF;
        colour.g = (protoColour.data() >> 8) & 0xFF;
        colour.b = (protoColour.data() >> 0) & 0xFF;

        colours.push_back(colour);
    }

    Modification::AnimationFrames animation;
    if (brickmap.has_animation()) {
        animation = readAnimation(brickmap.animation());
    }

    return std::make_tuple(serialInfo, brickgrid, brickmaps, colours, animation);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::BrickgridPtr>, std::vector<Generators::Brickmap>,
        std::vector<Generators::BrickmapColour>, Modification::AnimationFrames>>
loadBrickmap(std::filesystem::path directory)
{
    std::ifstream inputStream = loadBrickmapFile(directory);

    ASProto::Brickmap brickmap;
    brickmap.ParseFromIstream(&inputStream);

    return loadBrickmap(brickmap);
}

std::optional<
    std::tuple<SerialInfo, std::vector<Generators::BrickgridPtr>, std::vector<Generators::Brickmap>,
        std::vector<Generators::BrickmapColour>, Modification::AnimationFrames>>
loadBrickmap(const std::vector<uint8_t>& data)
{
    ASProto::Brickmap brickmap;
    brickmap.ParseFromArray(data.data(), data.size());

    return loadBrickmap(brickmap);
}

void storeBrickmap(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::BrickgridPtr> brickgrid, std::vector<Generators::Brickmap> brickmaps,
    std::vector<Generators::BrickmapColour> colours, Generators::GenerationInfo generationInfo,
    const Modification::AnimationFrames& animation)
{
    std::filesystem::path target = output / name / (name + ".voxbrick");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    ASProto::Brickmap brickmap;

    writeHeader(
        brickmap.mutable_header(), dimensions, generationInfo.voxelCount, generationInfo.nodes);

    for (const uint32_t& ptr : brickgrid) {
        brickmap.mutable_grid()->mutable_pointers()->Add(ptr);
    }

    for (const Generators::Brickmap& brick : brickmaps) {
        ASProto::Brick* protoBrick = brickmap.mutable_bricks()->Add();

        protoBrick->set_colour_ptr(brick.colourPtr);

        for (uint8_t i = 0; i < 8; i++) {
            protoBrick->mutable_occupancy()->Add(brick.occupancy[i]);
        }
    }

    for (const Generators::BrickmapColour& colour : colours) {
        ASProto::BrickColour* protoColour = brickmap.mutable_colours()->Add();

        protoColour->set_data((((uint32_t)colour.data) << 24) | (((uint32_t)colour.r) << 16)
            | (((uint32_t)colour.g) << 8) | (((uint32_t)colour.b) << 0));
    }

    if (animation.size() != 0) {
        writeAnimation(brickmap.mutable_animation(), animation);
    }

    brickmap.SerializeToOstream(&outputStream);

    outputStream.close();
}
}
