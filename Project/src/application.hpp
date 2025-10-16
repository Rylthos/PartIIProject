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

    VkFence fence;

    Image drawImage;
    VkDescriptorSet drawImageDescriptorSet;
};

struct PushConstants {
    glm::vec2 mousePos;
    bool renderFull;
};

class Application : public EventDispatcher {
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

    VkPipelineLayout m_VkPipelineLayout;
    VkPipeline m_VkPipeline;

    uint32_t m_CurrentFrameIndex { 0 };
    uint32_t m_CurrentSemaphore { 0 };

    glm::vec2 m_MousePos { 100, 0 };
    bool m_RenderFull = true;

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

    void createImGuiStructures();
    void destroyImGuiStructures();

    void createDescriptorPool();
    void createDescriptors();
    void destroyDescriptorPool();

    void createPipelineLayouts();
    void destroyPipelineLayouts();

    void createComputePipeline();
    void destroyComputePipeline();

    void renderUI();
    void UI(const Event& event);

    void render();

    void renderCompute(VkCommandBuffer& commandBuffer, const PerFrameData& currentFrame);
    void renderImGui(VkCommandBuffer& commandBuffer, VkImageView target, VkExtent2D extent);

    void update();

    void handleKeyInput(const Event& event);
    void handleMouse(const Event& event);
    void handleWindow(const Event& event);
};
