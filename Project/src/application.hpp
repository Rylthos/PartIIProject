#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include "logger.hpp"
#include "window.hpp"

struct Queue {
    VkQueue queue;
    uint32_t queueFamily;
};
class Application {
  public:
    void init();
    void start();

    void cleanup();

  private:
    Window m_Window;
    VkInstance m_VkInstance;
    VkDebugUtilsMessengerEXT m_VkDebugMessenger;
    VkSurfaceKHR m_VkSurface;

    VkPhysicalDevice m_VkPhysicalDevice;
    VkDevice m_VkDevice;

    Queue m_GraphicsQueue;

    VmaAllocator m_VmaAllocator;

  private:
    void initVulkan();
};
