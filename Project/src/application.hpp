#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include "logger.hpp"
#include "window.hpp"

#include "slang-com-helper.h"
#include "slang-com-ptr.h"
#include "slang.h"

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

    VkFence fence;

    Image drawImage;
    VkDescriptorSet drawImageDescriptorSet;
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
    VkExtent3D m_VkSwapchainImageExtent;
    std::vector<VkImage> m_VkSwapchainImages;
    std::vector<VkImageView> m_VkSwapchainImageViews;

    std::vector<VkSemaphore> m_RenderSemaphores;
    std::vector<VkSemaphore> m_SwapchainSemaphores;

    std::array<PerFrameData, FRAMES_IN_FLIGHT> m_PerFrameData;

    VkDescriptorPool m_VkDescriptorPool;
    VkDescriptorSetLayout m_ComputeDescriptorSetLayout;

    Slang::ComPtr<slang::IGlobalSession> m_GlobalSession;
    Slang::ComPtr<slang::ISession> m_Session;

    VkPipelineLayout m_VkPipelineLayout;
    VkPipeline m_VkPipeline;

    uint32_t m_CurrentFrameIndex { 0 };
    uint32_t m_CurrentSemaphore { 0 };

  private:
    void initVulkan();

    void createSwapchain();
    void destroySwapchain();

    void createDrawImages();
    void destroyDrawImages();

    void createCommandPools();
    void destroyCommandPools();

    void createSyncStructures();
    void destroySyncStructures();

    void createDescriptorPool();
    void createDescriptors();
    void destroyDescriptorPool();

    void setupSlang();

    void createPipelines();
    void destroyPipelines();

    void render();
    void update();
};
