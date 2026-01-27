#pragma once

#include <vector>

namespace Network {
struct Node {
    int socket;
    int clientSocket;
};

struct NetworkingInfo {
    bool enableServerSide = false;
    bool enableClientSide = false;

    bool networked;
};

}
