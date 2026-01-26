#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "node.hpp"

namespace Network {

Node initServer(uint16_t port);

Node initClient(const char* target, uint16_t port);

void cleanup(Node& node);

}
