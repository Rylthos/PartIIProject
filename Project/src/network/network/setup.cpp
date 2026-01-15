#include "setup.hpp"

#include "string.h"
#include <iostream>

namespace Network {

Node initServer(ServerSettings settings)
{
    Node server;

    server.socket = socket(settings.domain, settings.type, 0);

    if (server.socket < 0) {
        fprintf(stderr, "Failed to init server socket\n");
        exit(-1);
    }

    sockaddr_in socket;
    socket.sin_family = settings.domain;
    socket.sin_port = htons(settings.port);
    socket.sin_addr.s_addr = INADDR_ANY;

    if (bind(server.socket, (sockaddr*)&socket, sizeof(socket)) < 0) {
        fprintf(stderr, "Failed to bind to %d\n | %s\n", settings.port, strerror(errno));
        exit(-1);
    }

    listen(server.socket, 10);

    sockaddr_in client_addr {};
    socklen_t client_len = sizeof(client_addr);

    int client_fd = accept(server.socket, (sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        fprintf(stderr, "Failed to accept client\n | %s\n", strerror(errno));
        exit(-1);
    } else {
        server.clientSockets.push_back(client_fd);
    }

    return server;
}

Node initClient(ClientSettings settings)
{
    Node client;

    client.socket = socket(settings.domain, settings.type, 0);

    if (client.socket < 0) {
        fprintf(stderr, "Failed to init client socket\n");
        exit(-1);
    }

    sockaddr_in socket;
    socket.sin_family = settings.domain;
    socket.sin_port = htons(settings.port);
    inet_pton(settings.domain, settings.address.c_str(), &socket.sin_addr);

    if (connect(client.socket, (sockaddr*)&socket, sizeof(socket)) < 0) {
        fprintf(stderr, "Failed to connect to %s:%d\n", settings.address.c_str(), settings.port);
        exit(-1);
    }

    return client;
}
}
