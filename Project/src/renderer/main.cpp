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
    CLI::App cliApp { "Client settings for application" };
    argv = cliApp.ensure_utf8(argv);

    InitSettings settings;
    cliApp.add_flag("--enable-server-side", settings.enableServerSide, "Enable server side");
    cliApp.add_flag("--enable-client-side", settings.enableClientSide, "Enable client side");

    cliApp.add_option("-i,--ip", settings.targetIP, "The IP to target");
    cliApp.add_option("-p,--port", settings.targetPort, "The port to connect to");

    CLI11_PARSE(cliApp, argc, argv);

    settings.networked = settings.enableClientSide | settings.enableServerSide;

    Application app;

    app.init(settings);

    app.start();
    app.cleanup();

    return 0;
}
