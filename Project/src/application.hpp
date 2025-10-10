#pragma once

#include "vulkan/vulkan.h"

#include "logger.hpp"
#include "window.hpp"

class Application {
  public:
    void init();
    void start();

    void cleanup();

  private:
    Window m_Window;
    VkInstance m_VkInstance;

  private:
    void initVulkan();
};
