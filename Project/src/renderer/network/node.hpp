#pragma once

#include <optional>

namespace Network {
struct Node {
    int socket;
    std::optional<int> clientSocket;
};

struct NetworkingInfo {
    bool enableServerSide = false;
    bool enableClientSide = false;

    bool networked;
};

}
