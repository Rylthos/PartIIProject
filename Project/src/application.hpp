#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include "logger.hpp"
#include "window.hpp"

#define FRAMES_IN_FLIGHT 2

struct Queue {
    VkQueue queue;
    uint32_t queueFamily;
};

struct Image {
    VkImage image;
    VkImageView view;
    VkFormat format;
    VkExtent3D extent;
    VmaAllocation allocation;
};

struct PerFrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
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

    VkSwapchainKHR m_VkSwapchain;
    VkFormat m_VkSwapchainImageFormat;
    VkExtent2D m_VkSwapchainImageExtent;
    std::vector<VkImage> m_VkSwapchainImages;
    std::vector<VkImageView> m_VkSwapchainImageViews;

    Image m_DrawImage;

    std::array<PerFrameData, FRAMES_IN_FLIGHT> m_PerFrameData;

  private:
    void initVulkan();

    void createSwapchain();
    void destroySwapchain();

    void createDrawImages();
    void destroyDrawImages();

    void createCommandPools();
    void destroyCommandPools();
};
