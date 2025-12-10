#include "window.hpp"

#include "events/events.hpp"

#include "imgui.h"
#include "logger/logger.hpp"

#include "vulkan/vk_enum_string_helper.h"

#include <GLFW/glfw3.h>

void Window::init()
{
    if (!glfwInit()) {
        LOG_CRITICAL("Failed to initialise GLFW");
        exit(-1);
    }

    m_WindowSize = glm::ivec2 { 1600, 900 };

    LOG_DEBUG("Initialised GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_FALSE);
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "GLFW");
    glfwWindowHintString(GLFW_WAYLAND_APP_ID, "GLFW");

    m_Window
        = glfwCreateWindow(m_WindowSize.x, m_WindowSize.y, "Voxel Raymarching", nullptr, nullptr);

    if (!m_Window) {
        LOG_CRITICAL("Failed to create GLFW window");
        exit(-1);
    }

    glfwSetWindowUserPointer(m_Window, (void*)this);

    glfwSetKeyCallback(m_Window, handleKeyInput);
    glfwSetMouseButtonCallback(m_Window, handleMouseButton);
    glfwSetCursorEnterCallback(m_Window, handleMouseEnter);
    glfwSetCursorPosCallback(m_Window, handleMouseMove);
    glfwSetWindowSizeCallback(m_Window, handleWindowResize);

    LOG_DEBUG("Created GLFW window");
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

void Window::handleKeyInput(GLFWwindow* glfwWindow, int key, int scancode, int action, int mods)
{
    Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window->m_Window, GLFW_TRUE);
    }

    if (key == GLFW_KEY_LEFT_ALT && action == GLFW_PRESS) {
        int currentMode = glfwGetInputMode(window->m_Window, GLFW_CURSOR);
        if (currentMode == GLFW_CURSOR_DISABLED) {
            glfwSetInputMode(window->m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
        } else {
            glfwSetInputMode(window->m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
        }
    }

    if (ImGui::GetIO().WantCaptureKeyboard)
        return;

    if (action == GLFW_PRESS) {
        KeyboardPressEvent event {};
        event.keycode = key;
        event.mods = mods;
        window->post(event);
    } else if (action == GLFW_RELEASE) {
        KeyboardReleaseEvent event {};
        event.keycode = key;
        event.mods = mods;
        window->post(event);
    }
}

void Window::handleMouseButton(GLFWwindow* glfwWindow, int button, int action, int mods)
{
    Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);

    if (ImGui::GetIO().WantCaptureMouse)
        return;

    if (action == GLFW_PRESS) {
        MouseClickEvent event;
        event.button = button;
        window->post(event);
    } else if (action == GLFW_RELEASE) {
        MouseLiftEvent event;
        event.button = button;
        window->post(event);
    }
}

void Window::handleMouseMove(GLFWwindow* glfwWindow, double xPos, double yPos)
{
    Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);

    static double prevX;
    static double prevY;
    static bool initial = true;

    if (window->m_ResetDeltas) {
        initial = true;
        window->m_ResetDeltas = false;
    }

    if (initial) {
        prevX = xPos;
        prevY = yPos;

        initial = false;
    }

    double xDelta = xPos - prevX;
    double yDelta = yPos - prevY;

    prevX = xPos;
    prevY = yPos;

    int currentMode = glfwGetInputMode(window->m_Window, GLFW_CURSOR);
    if (currentMode == GLFW_CURSOR_DISABLED) {
        MouseMoveEvent event;
        event.position = { xPos, yPos };
        event.delta = { xDelta, yDelta };
        window->post(event);
    }
}

void Window::handleMouseEnter(GLFWwindow* glfwWindow, int entered)
{
    Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);

    if (!entered)
        window->m_ResetDeltas = true;

    MouseEnterExitEvent event;
    event.entered = entered;
    window->post(event);
}

void Window::handleWindowResize(GLFWwindow* glfwWindow, int width, int height)
{
    Window* window = (Window*)glfwGetWindowUserPointer(glfwWindow);

    window->m_WindowSize = glm::ivec2(width, height);

    WindowResizeEvent event;
    event.newSize = glm::ivec2(width, height);
    window->post(event);
}
