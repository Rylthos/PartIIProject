#include "window.hpp"

#include "logger.hpp"

#include <iostream>

void Window::init()
{
    if (!glfwInit()) {
        LOG_CRITICAL("Failed to initialise GLFW");
        exit(-1);
    }

    LOG_INFO("Initialised GLFW");

    m_Window = glfwCreateWindow(500, 500, "Voxel Raymarching", nullptr, nullptr);

    if (!m_Window) {
        LOG_CRITICAL("Failed to create GLFW window");
        exit(-1);
    }

    LOG_INFO("Created GLFW window");
}
