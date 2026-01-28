#include "contree.hpp"

#include "common.hpp"
#include "generators/contree.hpp"

#include "as_proto/contree.pb.h"

namespace Serializers {

std::ifstream loadContreeFile(std::filesystem::path directory)
{
    std::string foldername = directory.filename();

    std::filesystem::path file = directory / (foldername + ".voxcontree");
    std::ifstream inputStream(file.string(), std::ios::binary | std::ios::in);

    if (!inputStream.is_open()) {
        LOG_ERROR("Failed to open file: {}\n", file.string());
        return {};
    }

    return inputStream;
}

std::optional<std::tuple<SerialInfo, std::vector<Generators::ContreeNode>>> loadContree(
    ASProto::Contree& contree)
{
    SerialInfo serialInfo = readHeader(contree.header());

    size_t contreeNodes = contree.nodes_size();
    std::vector<Generators::ContreeNode> nodes;
    nodes.reserve(contreeNodes);

    for (size_t i = 0; i < contreeNodes; i++) {
        ASProto::ContreeNode node = contree.nodes().at(i);

        nodes.push_back(Generators::ContreeNode(node.high(), node.low()));
    }

    return std::make_pair(serialInfo, nodes);
}

std::optional<std::tuple<SerialInfo, std::vector<Generators::ContreeNode>>> loadContree(
    std::filesystem::path directory)
{
    std::ifstream inputStream = loadContreeFile(directory);
    ASProto::Contree contree;
    contree.ParseFromIstream(&inputStream);

    return loadContree(contree);
}
std::optional<std::tuple<SerialInfo, std::vector<Generators::ContreeNode>>> loadContree(
    const std::vector<uint8_t>& data)
{
    std::istringstream inputStream(std::string(data.begin(), data.end()));

    ASProto::Contree contree;
    contree.ParseFromArray(data.data(), data.size());

    return loadContree(contree);
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

    ASProto::Contree contree;
    writeHeader(
        contree.mutable_header(), dimensions, generationInfo.voxelCount, generationInfo.nodes);

    for (const auto& node : nodes) {
        ASProto::ContreeNode* protoNode = contree.mutable_nodes()->Add();
        std::array<uint64_t, 2> data = node.getData();
        protoNode->set_high(data[0]);
        protoNode->set_low(data[1]);
    }

    contree.SerializeToOstream(&outputStream);

    outputStream.close();
}
}
