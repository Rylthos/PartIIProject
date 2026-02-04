#pragma once

#include "logger/logger.hpp"

#include "buffer.hpp"
#include "camera.hpp"
#include "image.hpp"
#include "queue.hpp"
#include "window/base_window.hpp"

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <queue>
#include <vulkan/vulkan_core.h>

#include "network/node.hpp"

#define FRAMES_IN_FLIGHT 2

struct Sphere {
    glm::vec3 origin;
    float radius;
};

struct GBuffer {
    Image colours;
    Image normals;
    Image positions;
    Image depth;
};

struct PerFrameData {
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkFence fence;

    GBuffer gBuffer;

    Image drawImage;
    Image rayDirectionImage;

    Image networkImage;
    Buffer networkBuffer;

    VkDescriptorSet setupDescriptorSet;
    VkDescriptorSet gBufferDescriptorSet;
    VkDescriptorSet renderDescriptorSet;

    bool dirty = false;
};

struct InitSettings {
    Network::NetworkingInfo netInfo;

    bool serverDontWait = false;

    std::string targetIP;
    uint16_t targetPort;
};

class Application : public EventDispatcher {
  public:
    void init(InitSettings settings);

    void start();

    void cleanup();

  private:
    InitSettings m_Settings;

    std::jthread m_NetworkWriteLoop;

    std::unique_ptr<Window> m_Window;

    VkInstance m_VkInstance;
    VkDebugUtilsMessengerEXT m_VkDebugMessenger;
    VkSurfaceKHR m_VkSurface;

    VkPhysicalDevice m_VkPhysicalDevice;
    VkDevice m_VkDevice;

    std::shared_ptr<Queue> m_GraphicsQueue;

    VmaAllocator m_VmaAllocator;

    VkSwapchainKHR m_VkSwapchain;
    VkFormat m_VkSwapchainImageFormat;
    VkExtent3D m_VkSwapchainImageExtent;
    std::vector<VkImage> m_VkSwapchainImages;
    std::vector<VkImageView> m_VkSwapchainImageViews;

    std::vector<VkSemaphore> m_AcquireSemaphore;
    std::vector<VkSemaphore> m_SubmitSemaphore;

    Image m_ScreenshotImage;

    VkCommandPool m_GeneralPool;
    std::array<PerFrameData, FRAMES_IN_FLIGHT> m_PerFrameData;

    VkDescriptorPool m_VkDescriptorPool;

    VkDescriptorSetLayout m_SetupDescriptorLayout;
    VkDescriptorSetLayout m_GBufferDescriptorLayout;
    VkDescriptorSetLayout m_RenderDescriptorLayout;

    VkPipelineLayout m_VkSetupPipelineLayout;
    VkPipeline m_VkSetupPipeline;

    VkPipelineLayout m_VkRenderPipelineLayout;
    VkPipeline m_VkRenderPipeline;

    VkPipelineLayout m_VkUIPipelineLayout;
    VkPipeline m_VkUIPipeline;

    VkQueryPool m_VkQueryPool;
    double m_PreviousGPUTime = 0.;
    float m_TimestampInterval = 0.f;
    uint64_t m_PreviousGPUCount = 0;

    double m_PreviousFrameTime = 0.;

    uint32_t m_CurrentFrameIndex { 0 };

    Camera m_Camera;

    bool m_RenderImGui = true;

    std::optional<std::string> m_TakeScreenshot;

    std::queue<std::function<void()>> m_ThreadFunctions;
    std::mutex m_ThreadFunctionsMutex;

  private:
    void initVulkan();

    void createSwapchain();
    void destroySwapchain();

    void createImages();
    void createGBuffers();
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
    void createGBufferDescriptorLayout();
    void createRenderDescriptorLayout();
    void destroyDescriptorLayouts();

    void createSetupPipelineLayout();
    void destroySetupPipelineLayout();

    void createSetupPipeline();
    void destroySetupPipeline();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

    void createUIPipelineLayout();
    void destroyUIPipelineLayout();

    void createUIPipeline();
    void destroyUIPipeline();

    void createDescriptors();
    void createSetupDescriptor();
    void createGBufferDescriptor();
    void createRenderDescriptor();

    void destroyDescriptorPool();

    void createQueryPool();
    void destroyQueryPool();

    void addCallbacks();

    void requestUIRender();
    void UI(const Event& event);

    void render();
    void render_RayGeneration(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame);
    void render_ASRender(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame);
    void render_GBuffer(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame);
    void render_NetworkImage(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame);
    void render_Screenshot(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame);
    void render_UI(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame);
    void render_Present(
        VkCommandBuffer& commandBuffer, PerFrameData& currentFrame, uint32_t swapchainImageIndex);
    void render_FinaliseScreenshot();

    void renderImGui(VkCommandBuffer& commandBuffer, const PerFrameData& currentFrame);

    void update(float delta);

    void resize();

    void transmitNetworkImage(PerFrameData& frame);

    void handleKeyInput(const Event& event);
    void handleMouse(const Event& event);
    void handleWindow(const Event& event);

    void handleCameraEvent(const Event& event);

    void takeScreenshot(std::string filename);

    bool handleFrameReceive(const std::vector<uint8_t>& data, uint32_t messageID);
    bool handleUpdateReceive(const std::vector<uint8_t>& data, uint32_t messageID);

    bool serverSide()
    {
        return !m_Settings.netInfo.networked || m_Settings.netInfo.enableServerSide;
    }
    bool clientSide()
    {
        return !m_Settings.netInfo.networked || m_Settings.netInfo.enableClientSide;
    }
};
