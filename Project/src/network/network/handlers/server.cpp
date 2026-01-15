#include "server.hpp"

#include "../header.hpp"
#include "../serializers.hpp"

#include "string.h"

#define BUF_SIZE 1024

namespace Network::Server {

static ServerSettings s_Settings;

void fileEntryRequest(const Node& node, const std::vector<uint8_t>& data, size_t len, size_t& index)
{
    std::string path = Serializer::readString(data, len, index);

    printf("Request file entry");
    printf("\tPath: %s\n", path.c_str());
}

void handleRequest(const Node& node, const std::vector<uint8_t>& buffer, size_t len)
{
    size_t index = 0;
    Header header = parseHeader(buffer, len, index);

    switch (header.type) {
    case HeaderType::REQUEST_FILES:
        fileEntryRequest(node, buffer, len, index);
        break;
    default:
        fprintf(stderr, "Unhandled type: %d\n", static_cast<uint8_t>(header.type));
    }
}

void run(Node& node, ServerSettings settings)
{
    std::vector<uint8_t> buffer(BUF_SIZE);
    while (!node.clientSockets.empty()) {
        ssize_t read = recv(node.clientSockets[0], buffer.data(), BUF_SIZE, 0);

        if (read < 0) {
            fprintf(stderr, "Failed to read message\n | %s\n", strerror(errno));
            continue;
        }
        if (read == 0) {
            node.clientSockets.erase(node.clientSockets.begin() + 0);
            continue;
        }

        printf("Read: %ld\n", read);

        handleRequest(node, buffer, read);
    }
}

}
