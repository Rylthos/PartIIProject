#include "headless_window.hpp"

void HeadlessWindow::init() { p_WindowSize = { 400, 400 }; }
void HeadlessWindow::cleanup() { }

bool HeadlessWindow::shouldClose() { return m_ShouldClose; }
void HeadlessWindow::requestClose() { m_ShouldClose = true; }
