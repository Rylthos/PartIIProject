#pragma once

#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

class Window {
  public:
    void init();
    void cleanup();

    VkSurfaceKHR createSurface(const VkInstance& instance);

    GLFWwindow* getWindow() { return m_Window; }

    glm::uvec2 getWindowSize() { return m_WindowSize; }

  private:
    GLFWwindow* m_Window;

    glm::uvec2 m_WindowSize;
};
