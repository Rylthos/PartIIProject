#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <string>

#include <vector>

namespace Network {

struct Node {
    int socket;
    std::vector<int> clientSockets;
};

struct ServerSettings {
    int domain = AF_INET; // IPv4
    int type = SOCK_STREAM; // TCP
    uint16_t port;

    std::string resPath;
};

struct ClientSettings {
    int domain = AF_INET; // IPv4
    int type = SOCK_STREAM; // TCP
    uint16_t port;
    std::string address;
};

bool enabled();

Node initServer(ServerSettings settings);

Node initClient(ClientSettings settings);

}
