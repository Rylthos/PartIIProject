#ifdef DEBUG
#define VMA_DEBUG_LOG_FORMAT(format, ...)                                                          \
    do {                                                                                           \
        printf((format), __VA_ARGS__);                                                             \
        printf("\n");                                                                              \
    } while (false)
#endif

#define GLM_ENABLE_EXPERIMENTAL
#define VMA_IMPLEMENTATION

#include "application.hpp"

int main()
{
    Application app;
    app.init();
    app.start();
    app.cleanup();

    return 0;
}
