#pragma once

#include "GLFW/glfw3.h"

class Window {
  public:
    void init();

    GLFWwindow* getWindow() { return m_Window; }

  private:
    GLFWwindow* m_Window;
};
