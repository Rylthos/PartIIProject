#ifdef DEBUG
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                                          \
    do {                                                                                           \
        printf((format), __VA_ARGS__);                                                             \
        printf("\n");                                                                              \
    } while (false)
#endif

#define VMA_IMPLEMENTATION

#include "application.hpp"

#include "network/setup.hpp"
#include <CLI/CLI.hpp>

int main(int argc, char** argv)
{
    Network::ClientSettings settings;
    CLI::App cliApp { "Client settings for application" };
    argv = cliApp.ensure_utf8(argv);

    bool enabled = false;
    cliApp.add_flag("-e,--enable", enabled, "Enable networking");

    cliApp.add_option("-i,--ip", settings.address, "The IP to target");
    cliApp.add_option("-p,--port", settings.port, "The port to connect to");

    CLI11_PARSE(cliApp, argc, argv);

    Application app;

    if (enabled) {
        app.init(settings);
    } else {
        app.init();
    }

    app.start();
    app.cleanup();

    return 0;
}
