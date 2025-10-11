#include "window.hpp"

#include "vulkan/vk_enum_string_helper.h"

#include "logger.hpp"
#include <GLFW/glfw3.h>

void Window::init()
{
    if (!glfwInit()) {
        LOG_CRITICAL("Failed to initialise GLFW");
        exit(-1);
    }

    m_WindowSize = glm::ivec2 { 500, 500 };

    LOG_INFO("Initialised GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_Window
        = glfwCreateWindow(m_WindowSize.x, m_WindowSize.y, "Voxel Raymarching", nullptr, nullptr);

    if (!m_Window) {
        LOG_CRITICAL("Failed to create GLFW window");
        exit(-1);
    }

    LOG_INFO("Created GLFW window");
}

void Window::cleanup()
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

VkSurfaceKHR Window::createSurface(const VkInstance& instance)
{
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, m_Window, nullptr, &surface),
        "Failed to create window surface");
    return surface;
}

void Window::pollEvents() { glfwPollEvents(); }
void Window::swapBuffers() { glfwSwapBuffers(m_Window); }
bool Window::shouldClose() { return glfwWindowShouldClose(m_Window); };
