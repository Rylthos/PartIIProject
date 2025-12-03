#include "octree.hpp"
#include "generators/octree.hpp"
#include "serializers/common.hpp"

#include <iostream>

namespace Serializers {

std::pair<glm::uvec3, std::vector<Generators::OctreeNode>> loadOctree(
    std::filesystem::path directory)
{
    if (directory.has_filename()) {
        LOG_ERROR("Expected a directory not a file\n");
    }

    std::string foldername = directory.parent_path().filename();

    std::filesystem::path file = directory / (foldername + ".voxoctree");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        exit(-1);
    }

    glm::uvec3 dimensions = Serializers::readUvec3(inputStream);
    std::vector<Generators::OctreeNode> nodes;

    while (!inputStream.eof()) {
        uint32_t data = Serializers::readUint32(inputStream);

        nodes.push_back(Generators::OctreeNode(data));
    }
    return std::make_pair(dimensions, nodes);
}

void storeOctree(std::filesystem::path output, const std::string& name, glm::uvec3 dimensions,
    std::vector<Generators::OctreeNode> nodes)
{
    std::filesystem::path target = output / name / (name + ".voxoctree");

    std::ofstream outputStream(target.string(), std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputStream.is_open()) {
        fprintf(stderr, "Failed to open file %s\n", target.string().c_str());
        exit(-1);
    }

    writeUvec3(dimensions, outputStream);

    for (const auto& node : nodes) {
        uint32_t data = node.getData();
        Serializers::writeUint32(data, outputStream);
    }

    outputStream.close();

    std::cout << "Wrote " << target << "\n";
}
}
