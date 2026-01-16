#include "octree.hpp"

#include "common.hpp"

#include "generators/common.hpp"
#include "generators/octree.hpp"

#include <iostream>

namespace Serializers {

std::ifstream loadOctreeFile(std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxoctree");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    return inputStream;
}

std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    std::filesystem::path directory)
{
    std::ifstream inputStream = loadOctreeFile(directory);
    std::vector<uint8_t> data = vectorFromStream(inputStream);

    return loadOctree(data);
}

std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    const std::vector<uint8_t>& data)
{
    std::istringstream inputStream(std::string(data.begin(), data.end()));

    SerialInfo serialInfo;
    serialInfo.dimensions = Serializers::readUvec3(inputStream);
    serialInfo.voxels = Serializers::readUint64(inputStream);
    serialInfo.nodes = Serializers::readUint64(inputStream);

    std::vector<Generators::OctreeNode> nodes;
    nodes.reserve(serialInfo.nodes);

    while (!inputStream.eof()) {
        uint32_t data = Serializers::readUint32(inputStream);

        nodes.push_back(Generators::OctreeNode(data));
    }
    return std::make_pair(serialInfo, nodes);
}

void storeOctree(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::OctreeNode> nodes, Generators::GenerationInfo generationInfo)
{
    std::filesystem::path target = output / name / (name + ".voxoctree");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    writeUvec3(dimensions, outputStream);
    writeUint64(generationInfo.voxelCount, outputStream);
    writeUint64(generationInfo.nodes, outputStream);

    for (const auto& node : nodes) {
        uint32_t data = node.getData();
        Serializers::writeUint32(data, outputStream);
    }

    outputStream.close();
}
}
