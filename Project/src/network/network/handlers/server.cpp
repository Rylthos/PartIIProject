#include "server.hpp"

#include "../header.hpp"
#include "../serializers.hpp"

#include <filesystem>
#include <fstream>
#include <set>

#include "string.h"

#define BUF_SIZE 1024

namespace Network::Server {

static ServerSettings s_Settings;

void fileEntryRequest(uint32_t childFD, const Header& header, const std::vector<uint8_t>& data,
    size_t len, size_t& index)
{
    std::string path = Serializer::readString(data, len, index);

    printf("Request file entry");
    printf("\tPath: %s\n", path.c_str());

    std::filesystem::path basePath(s_Settings.resPath);

    if (path != "/") {
        basePath /= path;
    }
    printf("\tBase Path: %s\n", basePath.string().c_str());

    std::set<std::string> fileEntries;

    for (auto const& entry : std::filesystem::directory_iterator { basePath }) {
        if (entry.is_regular_file()) {
            fileEntries.insert(entry.path().extension());
        }
    }

    std::vector<uint8_t> contents;
    Serializer::writeUint32_t(fileEntries.size(), contents);
    for (const auto& str : fileEntries) {
        Serializer::writeString(str, contents);
    }

    std::vector<uint8_t> returnData;
    Header returnHeader;
    returnHeader.type = HeaderType::RETURN;
    returnHeader.id = header.id;
    returnHeader.size = contents.size();
    addHeader(returnHeader, returnData);

    returnData.insert(returnData.end(), contents.begin(), contents.end());

    send(childFD, returnData.data(), returnData.size(), 0);
}

void dirEntryRequest(uint32_t childFD, const Header& header, const std::vector<uint8_t>& data,
    size_t len, size_t& index)
{
    std::string path = Serializer::readString(data, len, index);

    printf("Request dir entries");
    printf("\tPath: %s\n", path.c_str());

    std::filesystem::path basePath(s_Settings.resPath);

    if (path != "/") {
        basePath /= path;
    }
    printf("\tBase Path: %s\n", basePath.string().c_str());

    std::vector<std::string> directories;

    for (auto const& entry : std::filesystem::directory_iterator { basePath }) {
        if (entry.is_directory()) {
            directories.push_back(entry.path().filename());
        }
    }

    std::vector<uint8_t> contents;
    Serializer::writeUint32_t(directories.size(), contents);
    for (const auto& str : directories) {
        Serializer::writeString(str, contents);
    }

    printf("\tContents Size: %ld\n", contents.size());

    std::vector<uint8_t> returnData;
    Header returnHeader;
    returnHeader.type = HeaderType::RETURN;
    returnHeader.id = header.id;
    returnHeader.size = contents.size();
    addHeader(returnHeader, returnData);

    returnData.insert(returnData.end(), contents.begin(), contents.end());
    printf("\tReturn data Size: %ld\n", returnData.size());
    printf("\tHeader: %d %d %d\n", (uint8_t)header.type, header.id, header.size);

    send(childFD, returnData.data(), returnData.size(), 0);
}

void sceneRequest(uint32_t childFD, const Header& header, const std::vector<uint8_t>& data,
    size_t len, size_t& index)
{
    std::string file = Serializer::readString(data, len, index);

    printf("Request Scene");

    std::filesystem::path basePath(s_Settings.resPath);
    basePath /= file;

    printf("\tPath: %s\n", basePath.c_str());

    std::ifstream input(basePath, std::ios::ate | std::ios::binary);
    if (!input) {
        fprintf(stderr, "Failed to read file: %s\n", basePath.string().c_str());
        return;
    }

    size_t size = input.tellg();
    input.seekg(0, std::ios::beg);

    std::vector<uint8_t> returnData;

    std::vector<uint8_t> fileData(size);
    input.read((char*)fileData.data(), fileData.size());
    input.close();

    Header returnHeader;
    returnHeader.type = HeaderType::RETURN;
    returnHeader.id = header.id;
    returnHeader.size = fileData.size();
    addHeader(returnHeader, returnData);

    returnData.insert(returnData.end(), fileData.begin(), fileData.end());

    printf("\tFile Data: %ld\n", fileData.size());
    printf("\tBuffer Size: %ld\n", returnData.size());

    send(childFD, returnData.data(), returnData.size(), 0);
}

void handleRequest(uint32_t childFD, const std::vector<uint8_t>& buffer, size_t len)
{
    size_t index = 0;
    Header header = parseHeader(buffer, len, index);

    switch (header.type) {
    case HeaderType::REQUEST_FILE_ENTRIES:
        fileEntryRequest(childFD, header, buffer, len, index);
        break;
    case HeaderType::REQUEST_DIR_ENTRIES:
        dirEntryRequest(childFD, header, buffer, len, index);
        break;
    case HeaderType::REQUEST_SCENE:
        sceneRequest(childFD, header, buffer, len, index);
        break;
    default:
        fprintf(stderr, "Unhandled type: %d\n", static_cast<uint8_t>(header.type));
    }
}

void run(Node& node, ServerSettings settings)
{
    s_Settings = settings;

    std::vector<uint8_t> buffer(BUF_SIZE);
    while (!node.clientSockets.empty()) {
        ssize_t read = recv(node.clientSockets[0], buffer.data(), BUF_SIZE, 0);

        if (read < 0) {
            fprintf(stderr, "Failed to read message\n | %s\n", strerror(errno));
            continue;
        }
        if (read == 0) {
            printf("Closing client connection: %d\n", node.clientSockets[0]);
            node.clientSockets.erase(node.clientSockets.begin() + 0);
            continue;
        }

        printf("Read: %ld\n", read);

        handleRequest(node.clientSockets[0], buffer, read);
    }
}

}
