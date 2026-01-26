#include "headless_window.hpp"

void HeadlessWindow::init() { p_WindowSize = { 400, 400 }; }
void HeadlessWindow::cleanup() { }
bool HeadlessWindow::shouldClose() { return false; }
