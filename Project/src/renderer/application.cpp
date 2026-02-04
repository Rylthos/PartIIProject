#include "application.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <ranges>
#include <vector>

#include <thread>

#include "CLI/CLI.hpp"
#include "animation_manager.hpp"
#include "buffer.hpp"
#include "compression.hpp"
#include "events/events.hpp"

#include "network/callbacks.hpp"
#include "network_proto/frame.pb.h"
#include "network_proto/header.pb.h"
#include "network_proto/update.pb.h"

#include "window/glfw_window.hpp"
#include "window/headless_window.hpp"

#include "network/loop.hpp"
#include "network/setup.hpp"

#include "stb/stb_image_write.h"

#include "VkBootstrap.h"
#include "acceleration_structure_manager.hpp"
#include "compute_pipeline.hpp"
#include "debug_utils.hpp"
#include "frame_commands.hpp"
#include "modification_manager.hpp"
#include "performance_logger.hpp"
#include "pipeline_layout.hpp"
#include "ring_buffer.hpp"
#include "scene_manager.hpp"
#include "shader_manager.hpp"
#include "tracing.hpp"

#include "glm/gtx/string_cast.hpp"
#include "glm/vector_relational.hpp"

#include <vulkan/vulkan_core.h>

#include <fstream>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "spdlog/fmt/bundled/base.h"
#include "spdlog/spdlog.h"

#define STORAGE_IMAGE_SIZE 1000
#define STORAGE_BUFFER_SIZE 1000

struct SetupPushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec3 cameraFront;
    alignas(16) glm::vec3 cameraRight;
    alignas(16) glm::vec3 cameraUp;
};

void Application::init(InitSettings settings)
{
    using namespace std::placeholders;

    m_Settings = settings;

    Logger::init();

    SceneManager::getManager()->init(m_Settings.netInfo);

    if (m_Settings.netInfo.networked) {
        if (m_Settings.netInfo.enableClientSide) {
            Network::initClient(settings.targetIP.c_str(), settings.targetPort);

            LOG_INFO("Connected to server: {}:{}", settings.targetIP, settings.targetPort);
        } else if (m_Settings.netInfo.enableServerSide) {

            Network::initServer(settings.targetPort, !m_Settings.serverDontWait);

            LOG_INFO("Setup server: Listening on port {}", settings.targetPort);
        }

        m_NetworkWriteLoop
            = std::jthread([&](std::stop_token stoken) { Network::writeLoop(stoken); });

        if (m_Settings.netInfo.enableClientSide) {
            Network::addCallback(NetProto::HEADER_TYPE_FRAME,
                std::bind(&Application::handleFrameReceive, this, _1, _2));

            Network::addCallback(NetProto::HEADER_TYPE_RETURN_DIR_ENTRIES,
                SceneManager::getManager()->getHandleReturnDirEntries());

            Network::addCallback(NetProto::HEADER_TYPE_RETURN_FILE_ENTRIES,
                SceneManager::getManager()->getHandleReturnFileEntries());
        }

        if (m_Settings.netInfo.enableServerSide) {
            Network::addCallback(NetProto::HEADER_TYPE_UPDATE,
                std::bind(&Application::handleUpdateReceive, this, _1, _2));

            Network::addCallback(NetProto::HEADER_TYPE_REQUEST_DIR_ENTRIES,
                SceneManager::getManager()->getHandleRequestDirEntries());

            Network::addCallback(NetProto::HEADER_TYPE_REQUEST_FILE_ENTRIES,
                SceneManager::getManager()->getHandleRequestFileEntries());

            Network::addCallback(
                NetProto::HEADER_TYPE_SET_AS, ASManager::getManager()->getHandleASChange());

            Network::addCallback(
                NetProto::HEADER_TYPE_LOAD_SCENE, SceneManager::getManager()->getHandleLoadScene());
        }
    }

    if (clientSide()) {
        m_Window = std::make_unique<GLFWWindow>();
    } else {
        m_Window = std::make_unique<HeadlessWindow>();
    }

    m_Window->init();

    initVulkan();

    ShaderManager::getInstance()->init(m_VkDevice);
    FrameCommands::getInstance()->init(m_VkDevice, m_VmaAllocator, m_GraphicsQueue);

    if (clientSide()) {
        createSwapchain();
    }

    createImages();

    createCommandPools();

    createSyncStructures();

    if (clientSide()) {
        createImGuiStructures();
    }

    createDescriptorPool();

    if (serverSide()) {
        createDescriptorLayouts();

        createSetupPipelineLayout();
        ShaderManager::getInstance()->addModule("ray_generation",
            std::bind(&Application::createSetupPipeline, this),
            std::bind(&Application::destroySetupPipeline, this));
        createSetupPipeline();

        createRenderPipelineLayout();
        ShaderManager::getInstance()->addModule("render",
            std::bind(&Application::createRenderPipeline, this),
            std::bind(&Application::destroyRenderPipeline, this));
        createRenderPipeline();

        createUIPipelineLayout();
        ShaderManager::getInstance()->addModule("ui",
            std::bind(&Application::createUIPipeline, this),
            std::bind(&Application::destroyUIPipeline, this));
        createUIPipeline();

        createDescriptors();
    }

    ASManager::getManager()->init({
        .device = m_VkDevice,
        .allocator = m_VmaAllocator,
        .graphicsQueue = m_GraphicsQueue,
        .descriptorPool = m_VkDescriptorPool,
        .commandPool = m_GeneralPool,
        .renderDescriptorLayout = m_GBufferDescriptorLayout,
        .netInfo = m_Settings.netInfo,
    });

    PerformanceLogger::getLogger()->init(&m_Camera);

    createQueryPool();

    addCallbacks();

    if (m_Settings.netInfo.enableClientSide) {
        NetProto::Update update;
        glm::uvec2 size = m_Window->getWindowSize();
        update.mutable_window_size()->set_x(size.x);
        update.mutable_window_size()->set_y(size.y);

        Network::sendMessage(NetProto::HEADER_TYPE_UPDATE, update);
    }

    LOG_DEBUG("Initialised application");
}

void Application::start()
{
    std::chrono::steady_clock timer;
    auto previous = timer.now();

    while (!m_Window->shouldClose()) {
        TRACE_FRAME_MARK;

        if (clientSide()) {
            GLFWWindow* glfwWindow = dynamic_cast<GLFWWindow*>(m_Window.get());
            assert(glfwWindow && "Expected window to be a glfw window");
            glfwWindow->pollEvents();
        }

        auto current = timer.now();
        std::chrono::duration<float, std::milli> difference = current - previous;
        float delta = difference.count() / 1000.f;
        previous = current;

        if (clientSide()) {
            requestUIRender();
        }

        render();

        update(delta);
    }
}

void Application::cleanup()
{
    Network::removeCallbacks();
    if (m_Settings.netInfo.networked) {
        m_NetworkWriteLoop.request_stop();
    }

    vkDeviceWaitIdle(m_VkDevice);

    ASManager::getManager()->cleanup();

    ShaderManager::getInstance()->cleanup();
    FrameCommands::getInstance()->cleanup();

    destroyQueryPool();

    if (serverSide()) {
        destroyUIPipelineLayout();
        destroyUIPipeline();
        destroyRenderPipelineLayout();
        destroyRenderPipeline();
        destroySetupPipelineLayout();
        destroySetupPipeline();

        destroyDescriptorLayouts();
    }

    destroyDescriptorPool();

    if (clientSide())
        destroyImGuiStructures();

    destroySyncStructures();

    destroyCommandPools();

    destroyImages();

    if (clientSide()) {
        destroySwapchain();
    }

    if (m_Settings.netInfo.networked) {
        Network::cleanup();
    }

    vmaDestroyAllocator(m_VmaAllocator);
    vkDestroyDevice(m_VkDevice, nullptr);

    if (clientSide()) {
        vkDestroySurfaceKHR(m_VkInstance, m_VkSurface, nullptr);
    }

    vkb::destroy_debug_utils_messenger(m_VkInstance, m_VkDebugMessenger);
    vkDestroyInstance(m_VkInstance, nullptr);

    m_Window->cleanup();

    LOG_DEBUG("Cleaned up");
}

void Application::initVulkan()
{
    LOG_DEBUG("Init Vulkan");
    vkb::InstanceBuilder builder;
    auto builderRet = builder.set_app_name("Voxel Raymarcher")
                          .request_validation_layers(true)
#ifdef DEBUG
                          .enable_validation_layers(true)
#endif
                          .use_default_debug_messenger()
                          .require_api_version(1, 4, 0)
                          .set_headless(m_Settings.netInfo.enableServerSide)
                          .build();

    vkb::Instance vkbInst = builderRet.value();
    m_VkInstance = vkbInst.instance;
    m_VkDebugMessenger = vkbInst.debug_messenger;
    if (clientSide()) {
        m_VkSurface = ((GLFWWindow*)m_Window.get())->createSurface(m_VkInstance);
    }

    VkPhysicalDeviceVulkan14Features features14 {};
    VkPhysicalDeviceVulkan13Features features13 {};
    features13.synchronization2 = true;
    features13.dynamicRendering = true;

    VkPhysicalDeviceVulkan12Features features12 {};
    features12.bufferDeviceAddress = true;
    features12.storageBuffer8BitAccess = true;
    features12.uniformAndStorageBuffer8BitAccess = true;
    features12.shaderInt8 = true;

    VkPhysicalDeviceVulkan11Features features11 {};
    VkPhysicalDeviceFeatures features {};
    features.shaderInt64 = true;

    vkb::PhysicalDeviceSelector selector { vkbInst };
    auto deviceSelector = selector.set_minimum_version(1, 4)
                              .set_required_features_14(features14)
                              .set_required_features_13(features13)
                              .set_required_features_12(features12)
                              .set_required_features_11(features11)
                              .set_required_features(features)
                              .add_required_extension("VK_KHR_shader_clock");

    if (clientSide()) {
        deviceSelector.set_surface(m_VkSurface);
    }

    auto vkbDeviceSelector = deviceSelector.select();

    if (!vkbDeviceSelector.has_value()) {
        LOG_CRITICAL(
            "{}: {}", vkbDeviceSelector.error().value(), vkbDeviceSelector.error().message());
        exit(-1);
    }

    VkPhysicalDeviceShaderClockFeaturesKHR clockFeatures {};
    clockFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_CLOCK_FEATURES_KHR;
    clockFeatures.shaderSubgroupClock = true;

    vkb::PhysicalDevice vkbPhysicalDevice = vkbDeviceSelector.value();
    vkb::DeviceBuilder deviceBuilder { vkbPhysicalDevice };
    vkb::Device vkbDevice = deviceBuilder.add_pNext(&clockFeatures).build().value();

    m_VkPhysicalDevice = vkbPhysicalDevice.physical_device;
    m_VkDevice = vkbDevice.device;

    VkQueue queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    uint32_t queueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    m_GraphicsQueue = std::make_shared<Queue>(queue, queueFamily);

    VmaAllocatorCreateInfo allocatorCI {};
    allocatorCI.instance = m_VkInstance;
    allocatorCI.physicalDevice = m_VkPhysicalDevice;
    allocatorCI.device = m_VkDevice;
    allocatorCI.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_VmaAllocator), "Failed to create allocator");

    Debug::setupDebugUtils(m_VkDevice);

    Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DEVICE, (uint64_t)m_VkDevice, "Device");
    Debug::setDebugName(
        m_VkDevice, VK_OBJECT_TYPE_QUEUE, (uint64_t)m_GraphicsQueue->getQueue(), "Graphics queue");
}

void Application::createSwapchain()
{
    vkb::SwapchainBuilder swapchainBuilder { m_VkPhysicalDevice, m_VkDevice, m_VkSurface };
    m_VkSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain
        = swapchainBuilder
              .set_desired_format({
                  .format = m_VkSwapchainImageFormat,
                  .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
              })
              .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
              .set_desired_extent(m_Window->getWindowSize().x, m_Window->getWindowSize().y)
              .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT)
              .build()
              .value();

    m_VkSwapchain = vkbSwapchain.swapchain;
    m_VkSwapchainImageExtent = {
        .width = vkbSwapchain.extent.width,
        .height = vkbSwapchain.extent.height,
        .depth = 1,
    };
    m_VkSwapchainImages = vkbSwapchain.get_images().value();
    m_VkSwapchainImageViews = vkbSwapchain.get_image_views().value();

    Debug::setDebugName(
        m_VkDevice, VK_OBJECT_TYPE_SWAPCHAIN_KHR, (uint64_t)m_VkSwapchain, "Swapchain");

    LOG_DEBUG("Created swapchain");
}

void Application::destroySwapchain()
{
    vkDestroySwapchainKHR(m_VkDevice, m_VkSwapchain, nullptr);

    for (size_t i = 0; i < m_VkSwapchainImageViews.size(); i++) {
        vkDestroyImageView(m_VkDevice, m_VkSwapchainImageViews[i], nullptr);
    }

    LOG_DEBUG("Destroyed swapchain");
}

void Application::createImages()
{
    createRayDirectionImages();
    createDrawImages();
    createGBuffers();
}

void Application::createGBuffers()
{
    VkExtent3D extent = { m_Window->getWindowSize().x, m_Window->getWindowSize().y, 1 };

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].gBuffer.positions.init(m_VkDevice, m_VmaAllocator,
            m_GraphicsQueue->getFamily(), extent, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_STORAGE_BIT);
        m_PerFrameData[i].gBuffer.positions.setDebugName("GBuffer positions image");
        m_PerFrameData[i].gBuffer.positions.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].gBuffer.positions.setDebugNameView("GBuffer positions image view");

        m_PerFrameData[i].gBuffer.colours.init(m_VkDevice, m_VmaAllocator,
            m_GraphicsQueue->getFamily(), extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_STORAGE_BIT);
        m_PerFrameData[i].gBuffer.colours.setDebugName("GBuffer colour image");
        m_PerFrameData[i].gBuffer.colours.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].gBuffer.colours.setDebugNameView("GBuffer colour image view");

        m_PerFrameData[i].gBuffer.normals.init(m_VkDevice, m_VmaAllocator,
            m_GraphicsQueue->getFamily(), extent, VK_FORMAT_R8G8B8A8_SNORM, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_STORAGE_BIT);
        m_PerFrameData[i].gBuffer.normals.setDebugName("GBuffer normals image");
        m_PerFrameData[i].gBuffer.normals.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].gBuffer.normals.setDebugNameView("GBuffer normals image view");

        m_PerFrameData[i].gBuffer.depth.init(m_VkDevice, m_VmaAllocator,
            m_GraphicsQueue->getFamily(), extent, VK_FORMAT_R16_SFLOAT, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_STORAGE_BIT);
        m_PerFrameData[i].gBuffer.depth.setDebugName("GBuffer depth image");
        m_PerFrameData[i].gBuffer.depth.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].gBuffer.depth.setDebugNameView("GBuffer depth image view");
    }
}

void Application::createDrawImages()
{
    VkExtent3D extent = { m_Window->getWindowSize().x, m_Window->getWindowSize().y, 1 };
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].drawImage.init(m_VkDevice, m_VmaAllocator, m_GraphicsQueue->getFamily(),
            extent, format, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        m_PerFrameData[i].drawImage.setDebugName("Draw image");

        m_PerFrameData[i].drawImage.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].drawImage.setDebugNameView("Draw image view");

        if (m_Settings.netInfo.networked) {
            m_PerFrameData[i].networkImage.init(m_VkDevice, m_VmaAllocator,
                m_GraphicsQueue->getFamily(), extent, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            m_PerFrameData[i].networkImage.setDebugName("Network Image");

            m_PerFrameData[i].networkBuffer.init(m_VkDevice, m_VmaAllocator,
                sizeof(uint32_t) * extent.width * extent.height,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VMA_MEMORY_USAGE_AUTO);
        }
    }

    m_ScreenshotImage.init(m_VkDevice, m_VmaAllocator, m_GraphicsQueue->getFamily(), extent,
        VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        VK_IMAGE_TILING_LINEAR);

    m_ScreenshotImage.setDebugName("Screenshot Image");

    LOG_DEBUG("Created draw images");
}

void Application::createRayDirectionImages()
{
    VkExtent3D extent = { m_Window->getWindowSize().x, m_Window->getWindowSize().y, 1 };
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].rayDirectionImage.init(m_VkDevice, m_VmaAllocator,
            m_GraphicsQueue->getFamily(), extent, format, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        m_PerFrameData[i].rayDirectionImage.setDebugName("Ray direction image");

        m_PerFrameData[i].rayDirectionImage.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].rayDirectionImage.setDebugNameView("Ray direction image view");
    }

    LOG_DEBUG("Created ray direction images");
}

void Application::destroyImages()
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].drawImage.cleanup();
        m_PerFrameData[i].rayDirectionImage.cleanup();

        m_PerFrameData[i].gBuffer.colours.cleanup();
        m_PerFrameData[i].gBuffer.depth.cleanup();
        m_PerFrameData[i].gBuffer.normals.cleanup();
        m_PerFrameData[i].gBuffer.positions.cleanup();

        if (m_Settings.netInfo.networked) {
            m_PerFrameData[i].networkImage.cleanup();
            m_PerFrameData[i].networkBuffer.cleanup();
        }
    }

    m_ScreenshotImage.cleanup();

    LOG_DEBUG("Destroyed images");
}

void Application::createCommandPools()
{
    VkCommandPoolCreateInfo commandPoolCI {};
    commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCI.pNext = nullptr;
    commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCI.queueFamilyIndex = m_GraphicsQueue->getFamily();

    VkCommandBufferAllocateInfo commandBufferAI {};
    commandBufferAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAI.pNext = nullptr;
    commandBufferAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAI.commandBufferCount = 1;

    VK_CHECK(vkCreateCommandPool(m_VkDevice, &commandPoolCI, nullptr, &m_GeneralPool),
        "Failed to create command pool");
    Debug::setDebugName(
        m_VkDevice, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)m_GeneralPool, "General command pool");

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(
                     m_VkDevice, &commandPoolCI, nullptr, &m_PerFrameData[i].commandPool),
            "Failed to create command pool");

        commandBufferAI.commandPool = m_PerFrameData[i].commandPool;
        VK_CHECK(vkAllocateCommandBuffers(
                     m_VkDevice, &commandBufferAI, &m_PerFrameData[i].commandBuffer),
            "Failed to allocate command buffer");

        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_COMMAND_POOL,
            (uint64_t)m_PerFrameData[i].commandPool, "Per frame command pool");
        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_COMMAND_BUFFER,
            (uint64_t)m_PerFrameData[i].commandBuffer, "Per frame command buffer");
    }

    LOG_DEBUG("Created command pools");
}

void Application::destroyCommandPools()
{
    vkDestroyCommandPool(m_VkDevice, m_GeneralPool, nullptr);
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(m_VkDevice, m_PerFrameData[i].commandPool, nullptr);
    }

    LOG_DEBUG("Destroyed command pools");
}

void Application::createSyncStructures()
{
    VkFenceCreateInfo fenceCI {};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.pNext = nullptr;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateFence(m_VkDevice, &fenceCI, nullptr, &m_PerFrameData[i].fence),
            "Failed to create fence");

        Debug::setDebugName(
            m_VkDevice, VK_OBJECT_TYPE_FENCE, (uint64_t)m_PerFrameData[i].fence, "Per frame fence");
    }

    VkSemaphoreCreateInfo semaphoreCI {};
    semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCI.pNext = nullptr;
    semaphoreCI.flags = 0;

    m_SubmitSemaphore.resize(m_VkSwapchainImages.size());
    m_AcquireSemaphore.resize(m_VkSwapchainImages.size());

    for (size_t i = 0; i < m_VkSwapchainImages.size(); i++) {
        VK_CHECK(vkCreateSemaphore(m_VkDevice, &semaphoreCI, nullptr, &m_SubmitSemaphore[i]),
            "Failed to create render semaphore");
        VK_CHECK(vkCreateSemaphore(m_VkDevice, &semaphoreCI, nullptr, &m_AcquireSemaphore[i]),
            "Failed to create swapchain semaphore");

        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)m_SubmitSemaphore[i],
            "Per swapchain render semaphore");
        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)m_AcquireSemaphore[i],
            "Per swapchain present semaphore");
    }

    LOG_DEBUG("Created sync structures");
}

void Application::destroySyncStructures()
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(m_VkDevice, m_PerFrameData[i].fence, nullptr);
    }

    for (size_t i = 0; i < m_VkSwapchainImages.size(); i++) {
        vkDestroySemaphore(m_VkDevice, m_SubmitSemaphore[i], nullptr);
        vkDestroySemaphore(m_VkDevice, m_AcquireSemaphore[i], nullptr);
    }

    LOG_DEBUG("Destroyed sync structures");
}

void Application::createImGuiStructures()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    ImVec4* colours = style.Colors;

    ImVec4 bgColour = colours[ImGuiCol_WindowBg];
    bgColour.w = 0.5;
    colours[ImGuiCol_WindowBg] = bgColour;

    GLFWWindow* glfwWindow = dynamic_cast<GLFWWindow*>(m_Window.get());
    assert(glfwWindow && "Expected window to be a glfw window");
    ImGui_ImplGlfw_InitForVulkan(glfwWindow->getWindow(), true);

    std::vector<VkFormat> formats = {
        m_PerFrameData[0].drawImage.getFormat(),
    };

    VkPipelineRenderingCreateInfo pipelineCI {};
    pipelineCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineCI.pNext = nullptr;
    pipelineCI.viewMask = 0;
    pipelineCI.colorAttachmentCount = formats.size();
    pipelineCI.pColorAttachmentFormats = formats.data();
    pipelineCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    pipelineCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    ImGui_ImplVulkan_InitInfo vulkanII {};
    vulkanII.ApiVersion = VK_API_VERSION_1_4;
    vulkanII.Instance = m_VkInstance;
    vulkanII.PhysicalDevice = m_VkPhysicalDevice;
    vulkanII.Device = m_VkDevice;
    vulkanII.QueueFamily = m_GraphicsQueue->getFamily();
    vulkanII.Queue = m_GraphicsQueue->getQueue();
    vulkanII.DescriptorPoolSize = 8;
    vulkanII.RenderPass = VK_NULL_HANDLE;
    vulkanII.MinImageCount = 3;
    vulkanII.ImageCount = 3;
    vulkanII.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    vulkanII.UseDynamicRendering = true;
    vulkanII.PipelineRenderingCreateInfo = pipelineCI;

    ImGui_ImplVulkan_Init(&vulkanII);
    LOG_DEBUG("Initialised ImGui");
}

void Application::destroyImGuiStructures()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    LOG_DEBUG("Destroyed ImGui");
}

void Application::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {
         .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = STORAGE_IMAGE_SIZE,
         },
        {
         .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = STORAGE_BUFFER_SIZE,
         },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI {};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.pNext = nullptr;
    descriptorPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolCI.maxSets = 1000,
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCI.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(m_VkDevice, &descriptorPoolCI, nullptr, &m_VkDescriptorPool),
        "Failed to create descriptor pool");

    Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_POOL, (uint64_t)m_VkDescriptorPool,
        "General descriptor pool");

    LOG_DEBUG("Created descriptor pool");
}

void Application::createDescriptorLayouts()
{
    createSetupDescriptorLayout();
    createGBufferDescriptorLayout();
    createRenderDescriptorLayout();
}

void Application::createSetupDescriptorLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
         .binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         }
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.pNext = nullptr;
    descriptorSetLayoutCI.flags = 0;
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorSetLayoutCI.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(
                 m_VkDevice, &descriptorSetLayoutCI, nullptr, &m_SetupDescriptorLayout),
        "Failed to create compute descriptor set layout");

    Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        (uint64_t)m_SetupDescriptorLayout, "Setup descriptor layout");
}

void Application::createGBufferDescriptorLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
         .binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         },
        {
         .binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         },
        {
         .binding = 2,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         },
        {
         .binding = 3,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         },
        {
         .binding = 4,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VK_CHECK(vkCreateDescriptorSetLayout(
                 m_VkDevice, &descriptorSetLayoutCI, nullptr, &m_GBufferDescriptorLayout),
        "Failed to create gBuffer descriptor set layout");

    Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        (uint64_t)m_GBufferDescriptorLayout, "GBuffer descriptor layout");
}

void Application::createRenderDescriptorLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
         .binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         }
    };
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.pNext = nullptr;
    descriptorSetLayoutCI.flags = 0;
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
    descriptorSetLayoutCI.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(
                 m_VkDevice, &descriptorSetLayoutCI, nullptr, &m_RenderDescriptorLayout),
        "Failed to create compute descriptor set layout");

    Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        (uint64_t)m_RenderDescriptorLayout, "Render descriptor layout");
}

void Application::destroyDescriptorLayouts()
{
    vkDestroyDescriptorSetLayout(m_VkDevice, m_SetupDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_VkDevice, m_GBufferDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_VkDevice, m_RenderDescriptorLayout, nullptr);
}

void Application::createSetupPipelineLayout()
{
    m_VkSetupPipelineLayout
        = PipelineLayoutGenerator::start(m_VkDevice)
              .addDescriptorLayout(m_SetupDescriptorLayout)
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SetupPushConstants))
              .setDebugName("Setup pipeline layout")
              .build();
}

void Application::destroySetupPipelineLayout()
{
    vkDestroyPipelineLayout(m_VkDevice, m_VkSetupPipelineLayout, nullptr);
}

void Application::createSetupPipeline()
{
    m_VkSetupPipeline = ComputePipelineGenerator::start(m_VkDevice, m_VkSetupPipelineLayout)
                            .setShader("ray_generation")
                            .setDebugName("Setup pipeline")
                            .build();
}

void Application::destroySetupPipeline()
{
    vkDestroyPipeline(m_VkDevice, m_VkSetupPipeline, nullptr);
}

void Application::createRenderPipelineLayout()
{
    m_VkRenderPipelineLayout
        = PipelineLayoutGenerator::start(m_VkDevice)
              .addDescriptorLayouts({ m_GBufferDescriptorLayout, m_RenderDescriptorLayout })
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SetupPushConstants))
              .setDebugName("Render layout")
              .build();
}

void Application::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(m_VkDevice, m_VkRenderPipelineLayout, nullptr);
}

void Application::createRenderPipeline()
{
    m_VkRenderPipeline = ComputePipelineGenerator::start(m_VkDevice, m_VkRenderPipelineLayout)
                             .setShader("render")
                             .setDebugName("Render pipeline")
                             .build();
}

void Application::destroyRenderPipeline()
{
    vkDestroyPipeline(m_VkDevice, m_VkRenderPipeline, nullptr);
}

void Application::createUIPipelineLayout()
{
    m_VkUIPipelineLayout = PipelineLayoutGenerator::start(m_VkDevice)
                               .addDescriptorLayout(m_RenderDescriptorLayout)
                               .setDebugName("UI pipeline layout")
                               .build();
}

void Application::destroyUIPipelineLayout()
{
    vkDestroyPipelineLayout(m_VkDevice, m_VkUIPipelineLayout, nullptr);
}

void Application::createUIPipeline()
{
    m_VkUIPipeline = ComputePipelineGenerator::start(m_VkDevice, m_VkUIPipelineLayout)
                         .setShader("ui")
                         .setDebugName("UI pipeline")
                         .build();
}

void Application::destroyUIPipeline() { vkDestroyPipeline(m_VkDevice, m_VkUIPipeline, nullptr); }

void Application::createDescriptors()
{
    createSetupDescriptor();
    createGBufferDescriptor();
    createRenderDescriptor();
}

void Application::createSetupDescriptor()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        descriptorSetLayouts.push_back(m_SetupDescriptorLayout);
    }

    VkDescriptorSetAllocateInfo descriptorSetAI {};
    descriptorSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAI.pNext = nullptr;
    descriptorSetAI.descriptorPool = m_VkDescriptorPool;
    descriptorSetAI.descriptorSetCount = descriptorSetLayouts.size();
    descriptorSetAI.pSetLayouts = descriptorSetLayouts.data();

    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.resize(FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(m_VkDevice, &descriptorSetAI, descriptorSets.data()),
        "Failed to allocate descriptor set");

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo imageInfo {};
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageView = m_PerFrameData[i].rayDirectionImage.getImageView();
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<VkWriteDescriptorSet> writeSets = {
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 0,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &imageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             },
        };

        vkUpdateDescriptorSets(m_VkDevice, writeSets.size(), writeSets.data(), 0, nullptr);

        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSets[i],
            "Setup descriptor");
    }

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].setupDescriptorSet = descriptorSets[i];
    }

    LOG_DEBUG("Created setup descriptors");
}

void Application::createGBufferDescriptor()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        descriptorSetLayouts.push_back(m_GBufferDescriptorLayout);
    }

    VkDescriptorSetAllocateInfo descriptorSetAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_VkDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.data(),
    };

    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.resize(FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(m_VkDevice, &descriptorSetAI, descriptorSets.data()),
        "Failed to allocate descriptor set");

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo positionImageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = m_PerFrameData[i].gBuffer.positions.getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkDescriptorImageInfo colourImageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = m_PerFrameData[i].gBuffer.colours.getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkDescriptorImageInfo normalImageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = m_PerFrameData[i].gBuffer.normals.getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkDescriptorImageInfo depthImageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = m_PerFrameData[i].gBuffer.depth.getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        VkDescriptorImageInfo rayDirectionImageInfo {
            .sampler = VK_NULL_HANDLE,
            .imageView = m_PerFrameData[i].rayDirectionImage.getImageView(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        };

        std::vector<VkWriteDescriptorSet> writeSets = {
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 0,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &positionImageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             },
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 1,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &colourImageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             },
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 2,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &normalImageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             },
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 3,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &depthImageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             },
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 4,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &rayDirectionImageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             },
        };

        vkUpdateDescriptorSets(m_VkDevice, writeSets.size(), writeSets.data(), 0, nullptr);

        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSets[i],
            "GBuffer descriptor");
    }

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].gBufferDescriptorSet = descriptorSets[i];
    }

    LOG_DEBUG("Created GBuffer descriptors");
}

void Application::createRenderDescriptor()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        descriptorSetLayouts.push_back(m_RenderDescriptorLayout);
    }

    VkDescriptorSetAllocateInfo descriptorSetAI {};
    descriptorSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAI.pNext = nullptr;
    descriptorSetAI.descriptorPool = m_VkDescriptorPool;
    descriptorSetAI.descriptorSetCount = descriptorSetLayouts.size();
    descriptorSetAI.pSetLayouts = descriptorSetLayouts.data();

    std::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.resize(FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(m_VkDevice, &descriptorSetAI, descriptorSets.data()),
        "Failed to allocate descriptor set");

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo renderImageInfo {};
        renderImageInfo.sampler = VK_NULL_HANDLE;
        renderImageInfo.imageView = m_PerFrameData[i].drawImage.getImageView();
        renderImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::vector<VkWriteDescriptorSet> writeSets = {
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 0,
             .dstArrayElement = 0,
             .descriptorCount = 1,
             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo = &renderImageInfo,
             .pBufferInfo = nullptr,
             .pTexelBufferView = nullptr,
             }
        };

        vkUpdateDescriptorSets(m_VkDevice, writeSets.size(), writeSets.data(), 0, nullptr);

        Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)descriptorSets[i],
            "Render descriptor");
    }

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].renderDescriptorSet = descriptorSets[i];
    }

    LOG_DEBUG("Created render descriptors");
}

void Application::destroyDescriptorPool()
{
    vkDestroyDescriptorPool(m_VkDevice, m_VkDescriptorPool, nullptr);

    LOG_DEBUG("Destroyed descriptor pool");
}

void Application::createQueryPool()
{
    VkQueryPoolCreateInfo queryPoolCI {};
    queryPoolCI.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCI.pNext = nullptr;
    queryPoolCI.flags = 0;
    queryPoolCI.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCI.queryCount = FRAMES_IN_FLIGHT * 4;
    queryPoolCI.pipelineStatistics = 0;

    VK_CHECK(vkCreateQueryPool(m_VkDevice, &queryPoolCI, nullptr, &m_VkQueryPool),
        "Failed to create query pool");

    Debug::setDebugName(
        m_VkDevice, VK_OBJECT_TYPE_QUERY_POOL, (uint64_t)m_VkQueryPool, "Query pool");

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_VkPhysicalDevice, &deviceProperties);
    m_TimestampInterval = deviceProperties.limits.timestampPeriod;
}

void Application::destroyQueryPool() { vkDestroyQueryPool(m_VkDevice, m_VkQueryPool, nullptr); }

void Application::addCallbacks()
{
    using namespace std::placeholders;

    if (m_Settings.netInfo.networked) {
        Network::setExitCallback([this]() { m_Window->requestClose(); });
    }

    m_Window->subscribe(EventFamily::KEYBOARD, std::bind(&Application::handleKeyInput, this, _1));
    m_Window->subscribe(EventFamily::MOUSE, std::bind(&Application::handleMouse, this, _1));
    m_Window->subscribe(EventFamily::WINDOW, std::bind(&Application::handleWindow, this, _1));
    m_Window->subscribe(EventFamily::MOUSE, ASManager::getManager()->getMouseEvent());

    subscribe(EventFamily::FRAME, std::bind(&Application::UI, this, _1));
    subscribe(EventFamily::FRAME, Logger::getFrameEvent());
    subscribe(EventFamily::FRAME, ASManager::getManager()->getUIEvent());
    subscribe(EventFamily::FRAME, SceneManager::getManager()->getUIEvent());
    subscribe(EventFamily::FRAME, PerformanceLogger::getLogger()->getFrameEvent());
    subscribe(EventFamily::FRAME, ModificationManager::getManager()->getUIEvent());
    subscribe(EventFamily::FRAME, AnimationManager::getManager()->getFrameEvent());

    PerformanceLogger::getLogger()->setScreenshotFunction(
        std::bind(&Application::takeScreenshot, this, _1));

    m_Window->subscribe(EventFamily::KEYBOARD, m_Camera.getKeyboardEvent());
    m_Window->subscribe(EventFamily::MOUSE, m_Camera.getMouseEvent());
    subscribe(EventFamily::FRAME, m_Camera.getFrameEvent());

    if (m_Settings.netInfo.enableClientSide) {
        m_Camera.subscribe(
            EventFamily::CAMERA, std::bind(&Application::handleCameraEvent, this, _1));
    }
}

void Application::requestUIRender()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    UIEvent event;
    post(event);

    ImGui::Render();
}

void Application::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);
    if (frameEvent.type() != FrameEventType::UI)
        return;

    if (ImGui::Begin("Timing")) {
        static RingBuffer<float, 100> previousFrames;

        previousFrames.pushBack(m_PreviousGPUTime);

        ImGui::Text("FPS                : %3f", 1.0f / (m_PreviousFrameTime));
        ImGui::Text("Previous Frame time: %6.2f ms", (m_PreviousFrameTime * 1000.f));
        ImGui::Spacing();
        ImGui::Text("GPU FPS            : %3f", 1.0f / (m_PreviousGPUTime / 1000.0f));
        ImGui::Text("Previous GPU time  : %6.2f ms", m_PreviousGPUTime);
        ImGui::Text("Previous GPU Count : %lu cyles", m_PreviousGPUCount);
        ImGui::Spacing();
        ImGui::Text("Frame times");
        ImGui::PlotLines("##Timing", previousFrames.getData().data(), previousFrames.getSize(), 0,
            nullptr, 0.0f, 50.0f, ImVec2(-1, 80.0f));
    }
    ImGui::End();

    if (ImGui::Begin("G Buffer")) {
        {
            std::vector<std::string> options = {
                "Full",
                "Positions",
                "Colours",
                "Normals",
                "Depth",
            };

            bool change = false;

            ImGui::Text("G Buffer rendering");
            static size_t selected = 0;
            size_t previousSelect = selected;
            if (ImGui::BeginCombo("##CurrentGBufferStyle", options[selected].c_str())) {
                for (uint8_t i = 0; i < options.size(); i++) {
                    const bool isSelected = (selected == i);
                    if (ImGui::Selectable(options[i].c_str(), isSelected)) {
                        selected = i;
                        change = true;
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            if (change) {
                switch (previousSelect) {
                case 0: // Full
                    break;
                case 1: // Position
                    ShaderManager::getInstance()->removeMacro("GBUFFER_RENDER_POS");
                    break;
                case 2: // Colours
                    ShaderManager::getInstance()->removeMacro("GBUFFER_RENDER_COL");
                    break;
                case 3: // Normals
                    ShaderManager::getInstance()->removeMacro("GBUFFER_RENDER_NOR");
                    break;
                case 4: // Depth
                    ShaderManager::getInstance()->removeMacro("GBUFFER_RENDER_DEP");
                    break;
                }

                switch (selected) {
                case 0: // Full
                    break;
                case 1: // Position
                    ShaderManager::getInstance()->defineMacro("GBUFFER_RENDER_POS");
                    break;
                case 2: // Colours
                    ShaderManager::getInstance()->defineMacro("GBUFFER_RENDER_COL");
                    break;
                case 3: // Normals
                    ShaderManager::getInstance()->defineMacro("GBUFFER_RENDER_NOR");
                    break;
                case 4: // Depth
                    ShaderManager::getInstance()->defineMacro("GBUFFER_RENDER_DEP");
                    break;
                }

                ShaderManager::getInstance()->moduleUpdated("render");
            }
        }
    }
    ImGui::End();

    ImGui::ShowDemoWindow();
}

void Application::render()
{
    PerFrameData& currentFrame = m_PerFrameData[m_CurrentFrameIndex];

    uint64_t timeout = 1e9;

    VkResult result = vkWaitForFences(m_VkDevice, 1, &currentFrame.fence, true, timeout);

    if (result != VK_SUCCESS) {
        LOG_ERROR("Fence timeout: {}", string_VkResult(result));
        return;
    }

    VK_CHECK(vkResetFences(m_VkDevice, 1, &currentFrame.fence), "Reset fence");

    if (m_Settings.netInfo.enableServerSide) {
        transmitNetworkImage(currentFrame);
    }

    uint32_t swapchainImageIndex = 0;

    if (clientSide()) {
        result = vkAcquireNextImageKHR(m_VkDevice, m_VkSwapchain, timeout,
            m_AcquireSemaphore[m_CurrentFrameIndex], nullptr, &swapchainImageIndex);
    }

    if (result == VK_NOT_READY) {
        return;
    }

    VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0), "Reset command Buffer");

    VkCommandBufferBeginInfo commandBufferBI {};
    commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBI.pNext = nullptr;
    commandBufferBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBI.pInheritanceInfo = nullptr;

    {
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBI), "Begin command buffer");

        currentFrame.drawImage.transition(
            commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        if (clientSide()) {
            Image::transition(commandBuffer, m_VkSwapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        }

        {
            Debug::beginCmdDebugLabel(commandBuffer, "Setup", { 0.f, 1.f, 0.f, 1.f });

            vkCmdResetQueryPool(commandBuffer, m_VkQueryPool, m_CurrentFrameIndex * 4, 4);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_VkQueryPool,
                m_CurrentFrameIndex * 4);

            Debug::endCmdDebugLabel(commandBuffer);
        }

        if (serverSide()) {
            render_RayGeneration(commandBuffer, currentFrame);

            render_ASRender(commandBuffer, currentFrame);

            render_GBuffer(commandBuffer, currentFrame);

            if (m_Settings.netInfo.enableServerSide) {
                render_NetworkImage(commandBuffer, currentFrame);
            }

            render_Screenshot(commandBuffer, currentFrame);

            render_UI(commandBuffer, currentFrame);
        } else if (currentFrame.dirty) {
            currentFrame.drawImage.transition(
                commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            currentFrame.networkImage.transition(
                commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            currentFrame.networkImage.copyToImage(commandBuffer, currentFrame.drawImage.getImage(),
                currentFrame.networkImage.getExtent(), currentFrame.drawImage.getExtent());

            currentFrame.drawImage.transition(
                commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
        }

        currentFrame.drawImage.transition(
            commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        if (clientSide()) {
            renderImGui(commandBuffer, currentFrame);
        }

        {
            Debug::beginCmdDebugLabel(commandBuffer, "Present", { 0.f, 1.f, 0.f, 1.f });
            currentFrame.drawImage.transition(commandBuffer,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            if (clientSide()) {
                currentFrame.drawImage.copyToImage(commandBuffer,
                    m_VkSwapchainImages[swapchainImageIndex], currentFrame.drawImage.getExtent(),
                    m_VkSwapchainImageExtent);

                Image::transition(commandBuffer, m_VkSwapchainImages[swapchainImageIndex],
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            }

            Debug::endCmdDebugLabel(commandBuffer);
        }

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkQueryPool,
            m_CurrentFrameIndex * 4 + 1);

        VK_CHECK(vkEndCommandBuffer(commandBuffer), "End command buffer");
    }

    render_Present(commandBuffer, currentFrame, swapchainImageIndex);

    {
        static uint64_t timeQueryBuffer[4];
        VkResult result = vkGetQueryPoolResults(m_VkDevice, m_VkQueryPool, m_CurrentFrameIndex * 4,
            4, sizeof(uint64_t) * 4, &timeQueryBuffer, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);

        if (result == VK_SUCCESS) {
            m_PreviousGPUTime
                = ((timeQueryBuffer[1] - timeQueryBuffer[0]) * m_TimestampInterval) / 1e6;
            m_PreviousGPUCount = timeQueryBuffer[3] - timeQueryBuffer[2];
        }
    }

    render_FinaliseScreenshot();

    if (clientSide()) {
        GLFWWindow* glfwWindow = dynamic_cast<GLFWWindow*>(m_Window.get());
        assert(glfwWindow && "Expected window to be a glfw window");
        glfwWindow->swapBuffers();
    }
}

void Application::render_RayGeneration(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame)
{
    Debug::beginCmdDebugLabel(commandBuffer, "Ray generation", { 0.f, 0.f, 1.f, 1.f });

    currentFrame.rayDirectionImage.transition(
        commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkSetupPipeline);

    std::vector<VkDescriptorSet> descriptorSets = {
        currentFrame.setupDescriptorSet,
    };

    SetupPushConstants setupPushConstant = {
        .cameraPosition = m_Camera.getPosition(),
        .cameraFront = m_Camera.getForwardVector(),
        .cameraRight = m_Camera.getRightVector(),
        .cameraUp = m_Camera.getUpVector(),
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkSetupPipelineLayout,
        0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(commandBuffer, m_VkSetupPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(SetupPushConstants), &setupPushConstant);

    vkCmdDispatch(commandBuffer, std::ceil(currentFrame.rayDirectionImage.getExtent().width / 8.f),
        std::ceil(currentFrame.rayDirectionImage.getExtent().height / 8.f), 1);

    Debug::endCmdDebugLabel(commandBuffer);
}

void Application::render_ASRender(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame)
{
    VkExtent2D imageSize = {
        .width = currentFrame.drawImage.getExtent().width,
        .height = currentFrame.drawImage.getExtent().height,
    };

    {
        VkImageMemoryBarrier2 imageBarrier = Image::memoryBarrier2(
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL, m_GraphicsQueue->getFamily(), m_GraphicsQueue->getFamily(),
            currentFrame.rayDirectionImage);

        VkDependencyInfo dependency = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .pNext = nullptr,
            .dependencyFlags = 0,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &imageBarrier,
        };

        vkCmdPipelineBarrier2(commandBuffer, &dependency);
    }

    currentFrame.gBuffer.positions.transition(
        commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    currentFrame.gBuffer.colours.transition(
        commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    currentFrame.gBuffer.normals.transition(
        commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    currentFrame.gBuffer.depth.transition(
        commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkQueryPool,
        m_CurrentFrameIndex * 4 + 2);

    ASManager::getManager()->render(
        commandBuffer, m_Camera, currentFrame.gBufferDescriptorSet, imageSize);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkQueryPool,
        m_CurrentFrameIndex * 4 + 3);
}

void Application::render_GBuffer(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame)
{
    VkImageMemoryBarrier2 positionBarrier = Image::memoryBarrier2(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL, m_GraphicsQueue->getFamily(), m_GraphicsQueue->getFamily(),
        currentFrame.gBuffer.positions);

    VkImageMemoryBarrier2 colourBarrier = Image::memoryBarrier2(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL, m_GraphicsQueue->getFamily(), m_GraphicsQueue->getFamily(),
        currentFrame.gBuffer.colours);

    VkImageMemoryBarrier2 normalsBarrier = Image::memoryBarrier2(
        VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL, m_GraphicsQueue->getFamily(), m_GraphicsQueue->getFamily(),
        currentFrame.gBuffer.normals);

    VkImageMemoryBarrier2 depthBarrier
        = Image::memoryBarrier2(VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_MEMORY_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            m_GraphicsQueue->getFamily(), m_GraphicsQueue->getFamily(), currentFrame.gBuffer.depth);

    std::vector<VkImageMemoryBarrier2> barriers
        = { positionBarrier, colourBarrier, normalsBarrier, depthBarrier };

    VkDependencyInfo dependency = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
        .memoryBarrierCount = 0,
        .pMemoryBarriers = nullptr,
        .bufferMemoryBarrierCount = 0,
        .pBufferMemoryBarriers = nullptr,
        .imageMemoryBarrierCount = static_cast<uint32_t>(barriers.size()),
        .pImageMemoryBarriers = barriers.data(),
    };

    vkCmdPipelineBarrier2(commandBuffer, &dependency);

    {
        Debug::beginCmdDebugLabel(commandBuffer, "Render", { 0.f, 0.f, 1.f, 1.f });

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkRenderPipeline);

        std::vector<VkDescriptorSet> descriptorSets = {
            currentFrame.gBufferDescriptorSet,
            currentFrame.renderDescriptorSet,
        };

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
            m_VkRenderPipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        vkCmdDispatch(commandBuffer, std::ceil(currentFrame.drawImage.getExtent().width / 8.f),
            std::ceil(currentFrame.drawImage.getExtent().height / 8.f), 1);

        Debug::endCmdDebugLabel(commandBuffer);
    }
}

void Application::render_NetworkImage(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame)
{
    currentFrame.networkImage.transition(
        commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    currentFrame.drawImage.copyToImage(commandBuffer, currentFrame.networkImage.getImage(),
        currentFrame.drawImage.getExtent(), currentFrame.networkImage.getExtent());

    currentFrame.networkImage.transition(
        commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    currentFrame.networkImage.copyToBuffer(commandBuffer, currentFrame.networkBuffer);
}

void Application::render_Screenshot(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame)
{
    if (m_TakeScreenshot.has_value()) {
        currentFrame.drawImage.transition(
            commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        m_ScreenshotImage.transition(
            commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        currentFrame.drawImage.copyToImage(commandBuffer, m_ScreenshotImage.getImage(),
            currentFrame.drawImage.getExtent(), m_ScreenshotImage.getExtent());

        currentFrame.drawImage.transition(
            commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    }
}

void Application::render_Present(
    VkCommandBuffer& commandBuffer, PerFrameData& currentFrame, uint32_t swapchainImageIndex)
{
    std::lock_guard lock(m_GraphicsQueue->getLock());

    VkCommandBufferSubmitInfo commandBufferSI {};
    commandBufferSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferSI.pNext = nullptr;
    commandBufferSI.commandBuffer = commandBuffer;
    commandBufferSI.deviceMask = 0;

    VkSemaphoreSubmitInfo waitSI;
    VkSemaphoreSubmitInfo signalSI;

    if (clientSide()) {
        waitSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSI.pNext = nullptr;
        waitSI.semaphore = m_AcquireSemaphore[m_CurrentFrameIndex];
        waitSI.value = 1;
        waitSI.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
        waitSI.deviceIndex = 0;

        signalSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSI.pNext = nullptr;
        signalSI.semaphore = m_SubmitSemaphore[swapchainImageIndex];
        signalSI.value = 1;
        signalSI.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        signalSI.deviceIndex = 0;
    }

    VkSubmitInfo2 submit {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.pNext = nullptr;
    submit.flags = 0;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &commandBufferSI;
    if (clientSide()) {
        submit.waitSemaphoreInfoCount = 1;
        submit.pWaitSemaphoreInfos = &waitSI;
        submit.signalSemaphoreInfoCount = 1;
        submit.pSignalSemaphoreInfos = &signalSI;
    }

    VK_CHECK(vkQueueSubmit2(m_GraphicsQueue->getQueue(), 1, &submit, currentFrame.fence),
        "Queue submit");

    if (clientSide()) {
        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_SubmitSemaphore[swapchainImageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_VkSwapchain;
        presentInfo.pImageIndices = &swapchainImageIndex;
        presentInfo.pResults = nullptr;
        vkQueuePresentKHR(m_GraphicsQueue->getQueue(), &presentInfo);
    }
}

void Application::render_FinaliseScreenshot()
{
    if (!m_TakeScreenshot.has_value())
        return;

    std::string value = m_TakeScreenshot.value();
    std::filesystem::path filename = value;
    m_TakeScreenshot.reset();

    {
        std::lock_guard lock(m_GraphicsQueue->getLock());
        vkQueueWaitIdle(m_GraphicsQueue->getQueue());
    }

    LOG_INFO("Take Screenshot");

    VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(
        m_VkDevice, m_ScreenshotImage.getImage(), &subResource, &subResourceLayout);

    uint8_t* data;
    vmaMapMemory(m_VmaAllocator, m_ScreenshotImage.getAllocation(), (void**)&data);

    data += subResourceLayout.offset;

    VkExtent3D imageExtent = m_ScreenshotImage.getExtent();
    glm::uvec2 size = { imageExtent.width, imageExtent.height };

    size_t totalBits = size.x * size.y * 4;

    std::vector<uint8_t> imageData;
    imageData.reserve(totalBits);

    for (uint32_t y = 0; y < size.y; y++) {
        uint32_t* row = (uint32_t*)data;
        for (uint32_t x = 0; x < size.x; x++) {
            imageData.push_back(((uint8_t*)row)[0]);
            imageData.push_back(((uint8_t*)row)[1]);
            imageData.push_back(((uint8_t*)row)[2]);
            imageData.push_back(((uint8_t*)row)[3]);
            row++;
        }
        data += subResourceLayout.rowPitch;
    }

    if (filename.has_parent_path()) {
        if (!std::filesystem::exists(std::filesystem::path(filename).parent_path())) {
            std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
        }
    }

    stbi_write_jpg(filename.string().c_str(), size.x, size.y, 4, imageData.data(), 95);

    LOG_INFO("Wrote screenshot: {}", filename.string());

    vmaUnmapMemory(m_VmaAllocator, m_ScreenshotImage.getAllocation());
}

void Application::render_UI(VkCommandBuffer& commandBuffer, PerFrameData& currentFrame)
{
    VkExtent2D imageSize = {
        .width = currentFrame.drawImage.getExtent().width,
        .height = currentFrame.drawImage.getExtent().height,
    };

    Debug::beginCmdDebugLabel(commandBuffer, "UI Rendering", { 0.f, 0.f, 1.f, 1.f });

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkUIPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        currentFrame.renderDescriptorSet,
    };

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkUIPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    vkCmdDispatch(
        commandBuffer, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    Debug::endCmdDebugLabel(commandBuffer);
}

void Application::renderImGui(VkCommandBuffer& commandBuffer, const PerFrameData& currentFrame)
{
    VkRenderingAttachmentInfo colourAI {};
    colourAI.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colourAI.pNext = nullptr;
    colourAI.imageView = currentFrame.drawImage.getImageView();
    colourAI.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colourAI.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colourAI.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderInfo {};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.pNext = nullptr;
    renderInfo.flags = 0;
    renderInfo.renderArea = VkRect2D {
        { 0, 0 },
        VkExtent2D {
         .width = currentFrame.drawImage.getExtent().width,
         .height = currentFrame.drawImage.getExtent().height,
         },
    };
    renderInfo.layerCount = 1;
    renderInfo.viewMask = 0;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colourAI;
    renderInfo.pDepthAttachment = nullptr;
    renderInfo.pStencilAttachment = nullptr;

    if (!m_RenderImGui)
        return;

    Debug::beginCmdDebugLabel(commandBuffer, "Render ImGui", { 1.f, 0.f, 0.f, 1.f });

    vkCmdBeginRendering(commandBuffer, &renderInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    vkCmdEndRendering(commandBuffer);

    Debug::endCmdDebugLabel(commandBuffer);
}

void Application::update(float delta)
{
    m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % FRAMES_IN_FLIGHT;

    m_PreviousFrameTime = delta;

    ShaderManager::getInstance()->updateShaders();
    FrameCommands::getInstance()->commit();

    ASManager::getManager()->update(delta);

    PerformanceLogger::getLogger()->addGPUTime(m_PreviousGPUTime);

    UpdateEvent event;
    event.delta = delta;
    post(event);

    if (!m_ThreadFunctions.empty()) {
        std::lock_guard _lock(m_ThreadFunctionsMutex);

        while (!m_ThreadFunctions.empty()) {
            (m_ThreadFunctions.front())();
            m_ThreadFunctions.pop();
        }
    }
}

void Application::resize()
{
    LOG_DEBUG("Resizing window");
    vkDeviceWaitIdle(m_VkDevice);

    if (serverSide()) {
        for (auto& frame : m_PerFrameData) {
            vkFreeDescriptorSets(m_VkDevice, m_VkDescriptorPool, 1, &frame.setupDescriptorSet);
            vkFreeDescriptorSets(m_VkDevice, m_VkDescriptorPool, 1, &frame.gBufferDescriptorSet);
            vkFreeDescriptorSets(m_VkDevice, m_VkDescriptorPool, 1, &frame.renderDescriptorSet);
        }
    }

    destroySyncStructures();
    destroyImages();

    if (clientSide()) {
        destroySwapchain();

        createSwapchain();
    }

    createImages();
    createSyncStructures();

    if (serverSide()) {
        createDescriptors();
    }

    if (clientSide()) {
        LOG_INFO("Window resize");
        NetProto::Update update;
        glm::uvec2 size = m_Window->getWindowSize();
        update.mutable_window_size()->set_x(size.x);
        update.mutable_window_size()->set_y(size.y);

        Network::sendMessage(NetProto::HEADER_TYPE_UPDATE, update);
    }
}

void Application::transmitNetworkImage(PerFrameData& frame)
{
    LOG_INFO("TRANSMIT");

    void* data = frame.networkBuffer.mapMemory();

    uint8_t* colourData = (uint8_t*)data;

    LOG_INFO("TOP_LEFT {} {} {}", colourData[0], colourData[1], colourData[2]);

    frame.networkBuffer.unmapMemory();
}

void Application::handleKeyInput(const Event& event)
{
    const KeyboardEvent& kEvent = static_cast<const KeyboardEvent&>(event);

    if (kEvent.type() == KeyboardEventType::PRESS) {
        const KeyboardPressEvent& keyEvent = static_cast<const KeyboardPressEvent&>(kEvent);

        if (keyEvent.keycode == GLFW_KEY_I) {
            m_RenderImGui = !m_RenderImGui;
        }

        if (keyEvent.keycode == GLFW_KEY_K) {
            takeScreenshot("screenshot.jpg");
        }
    }
}

void Application::handleMouse(const Event& event)
{
    const MouseEvent& mEvent = static_cast<const MouseEvent&>(event);

    if (mEvent.type() == MouseEventType::MOVE) { }
}

void Application::handleWindow(const Event& event)
{
    const WindowEvent& wEvent = static_cast<const WindowEvent&>(event);

    if (wEvent.type() == WindowEventType::RESIZE) {
        resize();
    }
}

void Application::handleCameraEvent(const Event& event)
{
    const CameraEvent& cEvent = static_cast<const CameraEvent&>(event);

    switch (cEvent.type()) {
    case CameraEventType::POSITION: {
        const CameraPositionEvent& pEvent = static_cast<const CameraPositionEvent&>(event);

        NetProto::Update update;
        update.mutable_camera_position()->set_x(pEvent.position.x);
        update.mutable_camera_position()->set_y(pEvent.position.y);
        update.mutable_camera_position()->set_z(pEvent.position.z);

        Network::sendMessage(NetProto::HEADER_TYPE_UPDATE, update);
        break;
    }
    case CameraEventType::ROTATION: {
        const CameraRotationEvent& pEvent = static_cast<const CameraRotationEvent&>(event);

        NetProto::Update update;
        update.mutable_camera_rotation()->set_x(pEvent.rotation.x);
        update.mutable_camera_rotation()->set_y(pEvent.rotation.y);

        Network::sendMessage(NetProto::HEADER_TYPE_UPDATE, update);
        break;
    }
    }
}

void Application::takeScreenshot(std::string filename) { m_TakeScreenshot = filename; }

bool Application::handleFrameReceive(const std::vector<uint8_t>& data, uint32_t messageID)
{
    static std::chrono::steady_clock clock;
    static auto previousTime = clock.now();

    static uint32_t previousID = 0;

    if (messageID < previousID) {
        return false;
    }

    NetProto::Frame frame;
    frame.ParseFromArray(data.data(), data.size());

    glm::uvec2 windowSize = m_Window->getWindowSize();
    if (frame.width() != windowSize.x || frame.height() != windowSize.y) {
        return false;
    }

    glm::uvec2 size { frame.width(), frame.height() };

    // std::vector<uint8_t> rawData(frame.data().begin(), frame.data().end());
    // std::vector<uint8_t> uncompressed = Compression::uncompress(rawData);

    // std::lock_guard _lock(m_NetworkImageMutex);
    // {
    //     VkImageSubresource subResource { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0 };
    //     VkSubresourceLayout subResourceLayout;
    //     vkGetImageSubresourceLayout(
    //         m_VkDevice, m_NetworkImage.getImage(), &subResource, &subResourceLayout);
    //
    //     uint8_t* imageData;
    //     vmaMapMemory(m_VmaAllocator, m_NetworkImage.getAllocation(), (void**)&imageData);
    //
    //     imageData += subResourceLayout.offset;
    //
    //     const uint32_t offset = 8;
    //
    //     for (uint32_t y = 0; y < size.y; y++) {
    //         uint32_t* row = (uint32_t*)imageData;
    //         for (uint32_t x = 0; x < size.x; x++) {
    //             uint32_t index = (x + y * size.x) * 4 + offset;
    //             ((uint8_t*)row)[0] = uncompressed[index + 0];
    //             ((uint8_t*)row)[1] = uncompressed[index + 1];
    //             ((uint8_t*)row)[2] = uncompressed[index + 2];
    //             ((uint8_t*)row)[3] = uncompressed[index + 3];
    //             row++;
    //         }
    //         imageData += subResourceLayout.rowPitch;
    //     }
    //
    //     vmaUnmapMemory(m_VmaAllocator, m_NetworkImage.getAllocation());
    // }

    // for (size_t i = 0; i < m_PerFrameData.size(); i++) {
    //     m_PerFrameData[i].dirty = true;
    // }

    {
        previousID = messageID;
        auto time = clock.now();

        std::chrono::duration<float, std::milli> diff = time - previousTime;
        m_PreviousGPUTime = diff.count();
        previousTime = time;
    }

    return true;
}

bool Application::handleUpdateReceive(const std::vector<uint8_t>& data, uint32_t messageID)
{
    NetProto::Update update;
    update.ParseFromArray(data.data(), data.size());

    if (update.has_window_size()) {
        std::lock_guard _lock(m_ThreadFunctionsMutex);
        m_ThreadFunctions.push([=, this]() {
            glm::uvec2 size;
            size.x = update.window_size().x();
            size.y = update.window_size().y();
            m_Window->setWindowSize(size);
        });
    }

    if (update.has_camera_position()) {
        std::lock_guard _lock(m_ThreadFunctionsMutex);
        m_ThreadFunctions.push([=, this]() {
            glm::vec3 position;
            position.x = update.camera_position().x();
            position.y = update.camera_position().y();
            position.z = update.camera_position().z();
            m_Camera.setPosition(position);
        });
    }

    if (update.has_camera_rotation()) {
        std::lock_guard _lock(m_ThreadFunctionsMutex);
        m_ThreadFunctions.push([=, this]() {
            glm::vec2 rotation;
            rotation.x = update.camera_rotation().x();
            rotation.y = update.camera_rotation().y();
            m_Camera.setRotation(rotation.x, rotation.y);
        });
    }

    return true;
}
