#include "contree.hpp"

#include "common.hpp"
#include "generators/contree.hpp"

#include <iostream>

namespace Serializers {

std::optional<std::tuple<SerialInfo, std::vector<Generators::ContreeNode>>> loadContree(
    std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxcontree");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    SerialInfo serialInfo;
    serialInfo.dimensions = Serializers::readUvec3(inputStream);
    serialInfo.voxels = Serializers::readUint64(inputStream);
    serialInfo.nodes = Serializers::readUint64(inputStream);

    std::vector<Generators::ContreeNode> nodes;
    nodes.reserve(serialInfo.nodes);

    while (!inputStream.eof()) {
        uint64_t high = Serializers::readUint64(inputStream);
        uint64_t low = Serializers::readUint64(inputStream);

        nodes.push_back(Generators::ContreeNode(high, low));
    }
    return std::make_pair(serialInfo, nodes);
}

void storeContree(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::ContreeNode> nodes, Generators::GenerationInfo generationInfo)
{
    std::filesystem::path target = output / name / (name + ".voxcontree");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    writeUvec3(dimensions, outputStream);
    writeUint64(generationInfo.voxelCount, outputStream);
    writeUint64(generationInfo.nodes, outputStream);

    for (const auto& node : nodes) {
        std::array<uint64_t, 2> data = node.getData();
        Serializers::writeUint64(data[0], outputStream);
        Serializers::writeUint64(data[1], outputStream);
    }

    outputStream.close();
}
}
