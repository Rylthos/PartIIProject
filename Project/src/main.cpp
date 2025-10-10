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
