#include <CLI/CLI.hpp>

#include <network/handlers/server.hpp>
#include <network/setup.hpp>

#include <cstdint>

int main(int argc, char** argv)
{
    CLI::App app { "Run server for renderer" };
    argv = app.ensure_utf8(argv);

    Network::ServerSettings settings;
    app.add_option("port", settings.port, "The port to connect to");
    app.add_option("res", settings.resPath, "Resource path");

    CLI11_PARSE(app, argc, argv);

    Network::Node server = Network::initServer(settings);

    printf("Connected to port %d\n", settings.port);

    Network::Server::run(server, settings);

    return 0;
}
