#pragma once

#include "vulkan/vulkan.h"

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

#include "event_watcher.hpp"

class Window : public EventWatcher {
  public:
    void init();
    void cleanup();

    VkSurfaceKHR createSurface(const VkInstance& instance);
    void pollEvents();
    void swapBuffers();
    bool shouldClose();

    GLFWwindow* getWindow() { return m_Window; }
    glm::uvec2 getWindowSize() { return m_WindowSize; }

  private:
    GLFWwindow* m_Window;

    glm::uvec2 m_WindowSize;

  private:
    static void handleKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
};
