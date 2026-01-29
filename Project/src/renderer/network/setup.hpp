#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "node.hpp"

namespace Network {

void initServer(uint16_t port, bool waitForClient = true);

void initClient(const char* target, uint16_t port);

void cleanup();

}
