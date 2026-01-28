#pragma once

#include "vulkan/vulkan.h"

#include "base_window.hpp"

#include "GLFW/glfw3.h"

class GLFWWindow : public Window {
  public:
    void init() override;
    void cleanup() override;

    bool shouldClose() override;
    void requestClose() override;

    void setWindowSize(glm::uvec2 windowSize) override;

    VkSurfaceKHR createSurface(const VkInstance& instance);
    void pollEvents();
    void swapBuffers();

    GLFWwindow* getWindow() { return m_Window; }

  private:
    GLFWwindow* m_Window;

    bool m_ResetDeltas = false;

  private:
    static void handleKeyInput(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void handleMouseButton(GLFWwindow* window, int button, int action, int mods);
    static void handleMouseMove(GLFWwindow* window, double xPos, double yPos);
    static void handleMouseEnter(GLFWwindow* window, int entered);

    static void handleWindowResize(GLFWwindow* window, int width, int height);
};
