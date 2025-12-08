#include "application.hpp"

#include <cstdint>
#include <functional>
#include <vector>

#include "events/events.hpp"

#include "VkBootstrap.h"
#include "acceleration_structure_manager.hpp"
#include "compute_pipeline.hpp"
#include "debug_utils.hpp"
#include "frame_commands.hpp"
#include "performance_logger.hpp"
#include "pipeline_layout.hpp"
#include "ring_buffer.hpp"
#include "scene_manager.hpp"
#include "shader_manager.hpp"
#include "tracing.hpp"

#include "glm/gtx/string_cast.hpp"
#include "glm/vector_relational.hpp"

#include <vulkan/vulkan_core.h>

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

void Application::init()
{
    using namespace std::placeholders;
    Logger::init();

    m_Window.init();
    initVulkan();

    ShaderManager::getInstance()->init(m_VkDevice);
    FrameCommands::getInstance()->init(
        m_VkDevice, m_VmaAllocator, m_GraphicsQueue.queue, m_GraphicsQueue.queueFamily);

    createSwapchain();
    createImages();

    createCommandPools();

    createSyncStructures();

    createImGuiStructures();

    createDescriptorPool();
    createDescriptorLayouts();

    ShaderManager::getInstance()->addModule("ray_generation",
        std::bind(&Application::createSetupPipeline, this),
        std::bind(&Application::destroySetupPipeline, this));
    createSetupPipelineLayout();
    createSetupPipeline();

    createDescriptors();

    ASManager::getManager()->init({
        .device = m_VkDevice,
        .allocator = m_VmaAllocator,
        .graphicsQueue = m_GraphicsQueue.queue,
        .graphicsQueueIndex = m_GraphicsQueue.queueFamily,
        .descriptorPool = m_VkDescriptorPool,
        .commandPool = m_GeneralPool,
        .renderDescriptorLayout = m_RenderDescriptorLayout,
    });

    createQueryPool();

    m_Window.subscribe(EventFamily::KEYBOARD, std::bind(&Application::handleKeyInput, this, _1));
    m_Window.subscribe(EventFamily::MOUSE, std::bind(&Application::handleMouse, this, _1));
    m_Window.subscribe(EventFamily::WINDOW, std::bind(&Application::handleWindow, this, _1));

    subscribe(EventFamily::FRAME, std::bind(&Application::UI, this, _1));
    subscribe(EventFamily::FRAME, Logger::getFrameEvent());
    subscribe(EventFamily::FRAME, ASManager::getManager()->getUIEvent());
    subscribe(EventFamily::FRAME, SceneManager::getManager()->getUIEvent());
    subscribe(EventFamily::FRAME, PerformanceLogger::getLogger()->getUIEvent());

    m_Window.subscribe(EventFamily::KEYBOARD, m_Camera.getKeyboardEvent());
    m_Window.subscribe(EventFamily::MOUSE, m_Camera.getMouseEvent());
    subscribe(EventFamily::FRAME, m_Camera.getFrameEvent());

    LOG_DEBUG("Initialised application");
}

void Application::start()
{
    std::chrono::steady_clock timer;
    auto previous = timer.now();

    while (!m_Window.shouldClose()) {
        TRACE_FRAME_MARK;

        m_Window.pollEvents();

        auto current = timer.now();
        std::chrono::duration<float, std::milli> difference = current - previous;
        float delta = difference.count() / 1000.f;
        previous = current;

        renderUI();
        render();
        update(delta);
    }
}

void Application::cleanup()
{
    vkDeviceWaitIdle(m_VkDevice);

    ASManager::getManager()->cleanup();

    ShaderManager::getInstance()->cleanup();
    FrameCommands::getInstance()->cleanup();

    destroyQueryPool();

    destroySetupPipelineLayout();
    destroySetupPipeline();

    destroyDescriptorLayouts();
    destroyDescriptorPool();

    destroyImGuiStructures();

    destroySyncStructures();

    destroyCommandPools();

    destroyImages();
    destroySwapchain();

    vmaDestroyAllocator(m_VmaAllocator);
    vkDestroyDevice(m_VkDevice, nullptr);
    vkDestroySurfaceKHR(m_VkInstance, m_VkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(m_VkInstance, m_VkDebugMessenger);
    vkDestroyInstance(m_VkInstance, nullptr);

    m_Window.cleanup();

    LOG_DEBUG("Cleaned up");
}

void Application::initVulkan()
{
    LOG_DEBUG("Init Vulkan");
    vkb::InstanceBuilder builder;
    auto builderRet = builder.set_app_name("Voxel Raymarcher")
                          .request_validation_layers(true)
                          .enable_validation_layers(true)
                          .use_default_debug_messenger()
                          .require_api_version(1, 4, 0)
                          .build();

    vkb::Instance vkbInst = builderRet.value();
    m_VkInstance = vkbInst.instance;
    m_VkDebugMessenger = vkbInst.debug_messenger;
    m_VkSurface = m_Window.createSurface(m_VkInstance);

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
    auto vkbDeviceSelector = selector.set_minimum_version(1, 4)
                                 .set_required_features_14(features14)
                                 .set_required_features_13(features13)
                                 .set_required_features_12(features12)
                                 .set_required_features_11(features11)
                                 .set_required_features(features)
                                 .set_surface(m_VkSurface)
                                 .add_required_extension("VK_KHR_shader_clock")
                                 .select();

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

    m_GraphicsQueue.queue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    m_GraphicsQueue.queueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorCI {};
    allocatorCI.instance = m_VkInstance;
    allocatorCI.physicalDevice = m_VkPhysicalDevice;
    allocatorCI.device = m_VkDevice;
    allocatorCI.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&allocatorCI, &m_VmaAllocator), "Failed to create allocator");

    Debug::setupDebugUtils(m_VkDevice);

    Debug::setDebugName(m_VkDevice, VK_OBJECT_TYPE_DEVICE, (uint64_t)m_VkDevice, "Device");
    Debug::setDebugName(
        m_VkDevice, VK_OBJECT_TYPE_QUEUE, (uint64_t)m_GraphicsQueue.queue, "Graphics queue");
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
              .set_desired_extent(m_Window.getWindowSize().x, m_Window.getWindowSize().y)
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
}

void Application::createDrawImages()
{
    VkExtent3D extent = { m_Window.getWindowSize().x, m_Window.getWindowSize().y, 1 };
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].drawImage.init(m_VkDevice, m_VmaAllocator, m_GraphicsQueue.queueFamily,
            extent, format, VK_IMAGE_TYPE_2D,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        m_PerFrameData[i].drawImage.setDebugName("Draw image");

        m_PerFrameData[i].drawImage.createView(VK_IMAGE_VIEW_TYPE_2D);
        m_PerFrameData[i].drawImage.setDebugNameView("Draw image view");
    }

    LOG_DEBUG("Created draw images");
}

void Application::createRayDirectionImages()
{
    VkExtent3D extent = { m_Window.getWindowSize().x, m_Window.getWindowSize().y, 1 };
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].rayDirectionImage.init(m_VkDevice, m_VmaAllocator,
            m_GraphicsQueue.queueFamily, extent, format, VK_IMAGE_TYPE_2D,
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
    }

    LOG_DEBUG("Destroyed images");
}

void Application::createCommandPools()
{
    VkCommandPoolCreateInfo commandPoolCI {};
    commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCI.pNext = nullptr;
    commandPoolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCI.queueFamilyIndex = m_GraphicsQueue.queueFamily;

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

    ImGui_ImplGlfw_InitForVulkan(m_Window.getWindow(), true);

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
    vulkanII.QueueFamily = m_GraphicsQueue.queueFamily;
    vulkanII.Queue = m_GraphicsQueue.queue;
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

void Application::createRenderDescriptorLayout()
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

void Application::createDescriptors()
{
    createSetupDescriptor();
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

        VkDescriptorImageInfo rayDirectionImageInfo {};
        rayDirectionImageInfo.sampler = VK_NULL_HANDLE;
        rayDirectionImageInfo.imageView = m_PerFrameData[i].rayDirectionImage.getImageView();
        rayDirectionImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

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
             },
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .pNext = nullptr,
             .dstSet = descriptorSets[i],
             .dstBinding = 1,
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

void Application::renderUI()
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

    uint32_t swapchainImageIndex = 0;
    result = vkAcquireNextImageKHR(m_VkDevice, m_VkSwapchain, timeout,
        m_AcquireSemaphore[m_CurrentFrameIndex], nullptr, &swapchainImageIndex);

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

        {
            Debug::beginCmdDebugLabel(commandBuffer, "Setup", { 0.f, 1.f, 0.f, 1.f });

            vkCmdResetQueryPool(commandBuffer, m_VkQueryPool, m_CurrentFrameIndex * 4, 4);

            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_VkQueryPool,
                m_CurrentFrameIndex * 4);

            Image::transition(commandBuffer, m_VkSwapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            currentFrame.drawImage.transition(
                commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            Debug::endCmdDebugLabel(commandBuffer);
        }

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

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                m_VkSetupPipelineLayout, 0, descriptorSets.size(), descriptorSets.data(), 0,
                nullptr);
            vkCmdPushConstants(commandBuffer, m_VkSetupPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
                0, sizeof(SetupPushConstants), &setupPushConstant);

            vkCmdDispatch(commandBuffer,
                std::ceil(currentFrame.rayDirectionImage.getExtent().width / 8.f),
                std::ceil(currentFrame.rayDirectionImage.getExtent().height / 8.f), 1);

            Debug::endCmdDebugLabel(commandBuffer);
        }

        {
            VkImageMemoryBarrier2 imageBarrier = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .pNext = nullptr,
                .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = m_GraphicsQueue.queueFamily,
                .dstQueueFamilyIndex = m_GraphicsQueue.queueFamily,
                .image = currentFrame.rayDirectionImage.getImage(),
                .subresourceRange = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = VK_REMAINING_MIP_LEVELS,
                    .baseArrayLayer = 0,
                    .layerCount = VK_REMAINING_MIP_LEVELS,
                },
            };

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

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkQueryPool,
            m_CurrentFrameIndex * 4 + 2);
        ASManager::getManager()->render(commandBuffer, m_Camera, currentFrame.renderDescriptorSet,
            {
                .width = currentFrame.drawImage.getExtent().width,
                .height = currentFrame.drawImage.getExtent().height,
            });
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkQueryPool,
            m_CurrentFrameIndex * 4 + 3);

        currentFrame.drawImage.transition(
            commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        renderImGui(commandBuffer, currentFrame);

        {
            Debug::beginCmdDebugLabel(commandBuffer, "Present", { 0.f, 1.f, 0.f, 1.f });
            currentFrame.drawImage.transition(commandBuffer,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            currentFrame.drawImage.copyToImage(commandBuffer,
                m_VkSwapchainImages[swapchainImageIndex], currentFrame.drawImage.getExtent(),
                m_VkSwapchainImageExtent);

            Image::transition(commandBuffer, m_VkSwapchainImages[swapchainImageIndex],
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            Debug::endCmdDebugLabel(commandBuffer);
        }

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_VkQueryPool,
            m_CurrentFrameIndex * 4 + 1);

        VK_CHECK(vkEndCommandBuffer(commandBuffer), "End command buffer");
    }

    {
        VkCommandBufferSubmitInfo commandBufferSI {};
        commandBufferSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferSI.pNext = nullptr;
        commandBufferSI.commandBuffer = commandBuffer;
        commandBufferSI.deviceMask = 0;

        VkSemaphoreSubmitInfo waitSI {};
        waitSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitSI.pNext = nullptr;
        waitSI.semaphore = m_AcquireSemaphore[m_CurrentFrameIndex];
        waitSI.value = 1;
        waitSI.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
        waitSI.deviceIndex = 0;

        VkSemaphoreSubmitInfo signalSI {};
        signalSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSI.pNext = nullptr;
        signalSI.semaphore = m_SubmitSemaphore[swapchainImageIndex];
        signalSI.value = 1;
        signalSI.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        signalSI.deviceIndex = 0;

        VkSubmitInfo2 submit {};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit.pNext = nullptr;
        submit.flags = 0;
        submit.waitSemaphoreInfoCount = 1;
        submit.pWaitSemaphoreInfos = &waitSI;
        submit.commandBufferInfoCount = 1;
        submit.pCommandBufferInfos = &commandBufferSI;
        submit.signalSemaphoreInfoCount = 1;
        submit.pSignalSemaphoreInfos = &signalSI;

        VK_CHECK(
            vkQueueSubmit2(m_GraphicsQueue.queue, 1, &submit, currentFrame.fence), "Queue submit");

        VkPresentInfoKHR presentInfo {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_SubmitSemaphore[swapchainImageIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_VkSwapchain;
        presentInfo.pImageIndices = &swapchainImageIndex;
        presentInfo.pResults = nullptr;
        vkQueuePresentKHR(m_GraphicsQueue.queue, &presentInfo);
    }

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

    m_Window.swapBuffers();
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

    UpdateEvent event;
    event.delta = delta;
    post(event);
}

void Application::handleKeyInput(const Event& event)
{
    const KeyboardEvent& kEvent = static_cast<const KeyboardEvent&>(event);

    if (kEvent.type() == KeyboardEventType::PRESS) {
        const KeyboardPressEvent& keyEvent = static_cast<const KeyboardPressEvent&>(kEvent);

        if (keyEvent.keycode == GLFW_KEY_I) {
            m_RenderImGui = !m_RenderImGui;
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
        LOG_DEBUG("Resizing window");
        vkDeviceWaitIdle(m_VkDevice);

        for (auto& frame : m_PerFrameData) {
            vkFreeDescriptorSets(m_VkDevice, m_VkDescriptorPool, 1, &frame.setupDescriptorSet);
            vkFreeDescriptorSets(m_VkDevice, m_VkDescriptorPool, 1, &frame.renderDescriptorSet);
        }

        destroySyncStructures();
        destroyImages();
        destroySwapchain();

        createSwapchain();
        createImages();
        createSyncStructures();
        createDescriptors();
    }
}
