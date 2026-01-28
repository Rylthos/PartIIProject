#include "octree.hpp"

#include "common.hpp"

#include "as_proto/octree.pb.h"

#include "generators/common.hpp"
#include "generators/octree.hpp"

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
    ASProto::Octree& octree)
{
    SerialInfo serialInfo = readHeader(octree.header());

    size_t nodeCount = octree.nodes_size();
    std::vector<Generators::OctreeNode> nodes;
    nodes.reserve(serialInfo.nodes);

    for (size_t i = 0; i < nodeCount; i++) {
        uint32_t value = octree.nodes().at(i).data();

        nodes.push_back(value);
    }

    return std::make_pair(serialInfo, nodes);
}

std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    std::filesystem::path directory)
{
    std::ifstream inputStream = loadOctreeFile(directory);
    ASProto::Octree octree;
    octree.ParseFromIstream(&inputStream);

    return loadOctree(octree);
}

std::optional<std::tuple<SerialInfo, std::vector<Generators::OctreeNode>>> loadOctree(
    const std::vector<uint8_t>& data)
{
    ASProto::Octree octree;
    octree.ParseFromArray(data.data(), data.size());

    return loadOctree(octree);
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

    ASProto::Octree octree;

    writeHeader(
        octree.mutable_header(), dimensions, generationInfo.voxelCount, generationInfo.nodes);

    for (const auto& node : nodes) {
        uint32_t data = node.getData();

        ASProto::OctreeNode* protoNode = octree.mutable_nodes()->Add();
        protoNode->set_data(data);
    }

    octree.SerializeToOstream(&outputStream);

    outputStream.close();
}
}
