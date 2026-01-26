#pragma once

#include "vulkan/vulkan.h"

#include "events/event_dispatcher.hpp"

#include "GLFW/glfw3.h"
#include "glm/glm.hpp"

class Window : public EventDispatcher {
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

    bool m_ResetDeltas = false;

    glm::uvec2 m_WindowSize { 1, 1 };

  private:
    static void handleKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void handleMouseButton(GLFWwindow* window, int button, int action, int mods);
    static void handleMouseMove(GLFWwindow* window, double xPos, double yPos);
    static void handleMouseEnter(GLFWwindow* window, int entered);

    static void handleWindowResize(GLFWwindow* window, int width, int height);
};
