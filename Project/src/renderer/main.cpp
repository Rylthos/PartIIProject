#ifdef DEBUG
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                                          \
    do {                                                                                           \
        printf((format), __VA_ARGS__);                                                             \
        printf("\n");                                                                              \
    } while (false)
#endif

#define VMA_IMPLEMENTATION

// #define SERVER_CLIENT

#include "application.hpp"

#ifdef SERVER_CLIENT
#include "network/setup.hpp"

#include <CLI/CLI.hpp>
#endif

int main(int argc, char** argv)
{
#ifdef SERVER_CLIENT
    Network::ClientSettings settings;
    CLI::App cliApp { "Client settings for application" };
    argv = cliApp.ensure_utf8(argv);

    cliApp.add_option("ip", settings.address, "The IP to target");
    cliApp.add_option("port", settings.port, "The port to connect to");

    CLI11_PARSE(cliApp, argc, argv);
#endif

    Application app;
#ifdef SERVER_CLIENT
    app.init(settings);
#else
    app.init();
#endif
    app.start();
    app.cleanup();

    return 0;
}
