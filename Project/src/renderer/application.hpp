#pragma once

#include "logger/logger.hpp"

#include "camera.hpp"
#include "image.hpp"
#include "window.hpp"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#define FRAMES_IN_FLIGHT 2

struct Sphere {
    glm::vec3 origin;
    float radius;
};

struct Queue {
    VkQueue queue;
    uint32_t queueFamily;
};

struct PerFrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkFence fence;

    Image drawImage;
    Image rayDirectionImage;
    VkDescriptorSet setupDescriptorSet;
    VkDescriptorSet renderDescriptorSet;
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

    std::vector<VkSemaphore> m_AcquireSemaphore;
    std::vector<VkSemaphore> m_SubmitSemaphore;

    VkCommandPool m_GeneralPool;
    std::array<PerFrameData, FRAMES_IN_FLIGHT> m_PerFrameData;

    VkDescriptorPool m_VkDescriptorPool;

    VkDescriptorSetLayout m_SetupDescriptorLayout;
    VkDescriptorSetLayout m_RenderDescriptorLayout;

    VkPipelineLayout m_VkSetupPipelineLayout;
    VkPipeline m_VkSetupPipeline;

    VkQueryPool m_VkQueryPool;
    double m_PreviousGPUTime = 0.;
    float m_TimestampInterval = 0.f;
    uint64_t m_PreviousGPUCount = 0;

    double m_PreviousFrameTime = 0.;

    uint32_t m_CurrentFrameIndex { 0 };

    Camera m_Camera;

    bool m_RenderImGui = true;

  private:
    void initVulkan();

    void createSwapchain();
    void destroySwapchain();

    void createImages();
    void createDrawImages();
    void createRayDirectionImages();
    void destroyImages();

    void createCommandPools();
    void destroyCommandPools();

    void createSyncStructures();
    void destroySyncStructures();

    void createImGuiStructures();
    void destroyImGuiStructures();

    void createDescriptorPool();

    void createDescriptorLayouts();
    void createSetupDescriptorLayout();
    void createRenderDescriptorLayout();
    void destroyDescriptorLayouts();

    void createSetupPipelineLayout();
    void destroySetupPipelineLayout();

    void createSetupPipeline();
    void destroySetupPipeline();

    void createDescriptors();
    void createSetupDescriptor();
    void createRenderDescriptor();

    void destroyDescriptorPool();

    void createQueryPool();
    void destroyQueryPool();

    void renderUI();
    void UI(const Event& event);

    void render();

    void renderImGui(VkCommandBuffer& commandBuffer, const PerFrameData& currentFrame);

    void update(float delta);

    void handleKeyInput(const Event& event);
    void handleMouse(const Event& event);
    void handleWindow(const Event& event);
};
