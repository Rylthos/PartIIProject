#define GLFW_INCLUDE_VULKAN
#define GLM_ENABLE_EXPERIMENTAL

#include "application.hpp"

int main()
{
    Application app;
    app.init();
    app.start();
    app.cleanup();

    return 0;
}
