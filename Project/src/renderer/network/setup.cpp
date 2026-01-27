#include "setup.hpp"

#include "string.h"
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>

#include "logger/logger.hpp"

namespace Network {

Node initServer(uint16_t port, bool waitForClient)
{
    Node server;

    server.socket = socket(AF_INET, SOCK_STREAM, 0);

    if (server.socket < 0) {
        LOG_CRITICAL("Failed to init server side socket");
        exit(-1);
    }

    sockaddr_in socket;
    socket.sin_family = AF_INET, socket.sin_port = htons(port);
    socket.sin_addr.s_addr = INADDR_ANY;

    if (bind(server.socket, (sockaddr*)&socket, sizeof(socket)) < 0) {
        LOG_CRITICAL("Failed to bind server to {} | {}", port, strerror(errno));
        exit(-1);
    }

    listen(server.socket, 10);

    if (waitForClient) {
        sockaddr_in client_addr {};
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server.socket, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            LOG_CRITICAL("Failed to accept client: {}", strerror(errno));
            exit(-1);
        } else {
            server.clientSocket = client_fd;
        }
    }

    return server;
}

Node initClient(const char* target, uint16_t port)
{
    Node client;

    client.socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client.socket < 0) {
        LOG_CRITICAL("Failed to init client side socket");
        exit(-1);
    }

    sockaddr_in socket;
    socket.sin_family = AF_INET;
    socket.sin_port = htons(port);
    inet_pton(AF_INET, target, &socket.sin_addr);

    if (connect(client.socket, (sockaddr*)&socket, sizeof(socket)) < 0) {
        LOG_CRITICAL("Failed to init client to {}:{}", target, port);
        exit(-1);
    }

    return client;
}

void cleanup(Node& node) { close(node.socket); }

}
