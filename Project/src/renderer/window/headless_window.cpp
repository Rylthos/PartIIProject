#include "headless_window.hpp"

void HeadlessWindow::init() { p_WindowSize = { 400, 400 }; }
void HeadlessWindow::cleanup() { }

bool HeadlessWindow::shouldClose() { return m_ShouldClose; }
void HeadlessWindow::requestClose() { m_ShouldClose = true; }

void HeadlessWindow::setWindowSize(glm::uvec2 windowSize)
{
    if (p_WindowSize == windowSize)
        return;

    p_WindowSize = windowSize;

    WindowResizeEvent event;
    event.newSize = glm::ivec2(windowSize.x, windowSize.y);
    post(event);
}
