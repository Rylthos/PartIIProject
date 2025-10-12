#include "application.hpp"

#include "VkBootstrap.h"

#include <vector>

#define SLANG_DIAG(diagnostics)                                                                    \
    do {                                                                                           \
        if ((diagnostics) != nullptr) {                                                            \
            LOG_ERROR((const char*)(diagnostics)->getBufferPointer());                             \
        }                                                                                          \
    } while (0)

VkShaderModule createShaderModule(
    VkDevice device, std::string filename, Slang::ComPtr<slang::ISession> session)
{
    LOG_INFO("Create shader module");

    Slang::ComPtr<slang::IModule> slangModule;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        slangModule = session->loadModule(filename.c_str(), diagnostics.writeRef());
        SLANG_DIAG(diagnostics);
    }

    Slang::ComPtr<slang::IEntryPoint> entryPoint;
    slangModule->findEntryPointByName("computeMain", entryPoint.writeRef());

    if (!entryPoint)
        LOG_ERROR("Error getting entry point");

    std::array<slang::IComponentType*, 2> componentTypes = { slangModule, entryPoint };

    Slang::ComPtr<slang::IComponentType> composedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result = session->createCompositeComponentType(componentTypes.data(),
            componentTypes.size(), composedProgram.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail composing shader");
        }
    }

    Slang::ComPtr<slang::IComponentType> linkedProgram;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result
            = composedProgram->link(linkedProgram.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail composing shader");
        }
    }

    Slang::ComPtr<slang::IBlob> spirvCode;
    {
        Slang::ComPtr<slang::IBlob> diagnostics;
        SlangResult result
            = linkedProgram->getTargetCode(0, spirvCode.writeRef(), diagnostics.writeRef());

        SLANG_DIAG(diagnostics);
        if (SLANG_FAILED(result)) {
            LOG_ERROR("Fail linking shader");
        }
    }
    VkShaderModuleCreateInfo moduleCI {};
    moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCI.pNext = nullptr;
    moduleCI.flags = 0;
    moduleCI.codeSize = spirvCode->getBufferSize();
    moduleCI.pCode = (uint32_t*)spirvCode->getBufferPointer();

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &moduleCI, nullptr, &module),
        "Failed to create shader module");

    LOG_INFO("Compiled shader module: {}", filename);

    return module;
}

void transitionImage(VkCommandBuffer commandBuffer, VkImage target, VkImageLayout currentLayout,
    VkImageLayout targetLayout)
{
    VkImageMemoryBarrier2 imageBarrier {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = targetLayout;
    imageBarrier.image = target;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dependencyInfo {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.memoryBarrierCount = 0;
    dependencyInfo.pMemoryBarriers = nullptr;
    dependencyInfo.bufferMemoryBarrierCount = 0;
    dependencyInfo.pBufferMemoryBarriers = nullptr;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void copyImageToImage(
    VkCommandBuffer command, VkImage src, VkImage dst, VkExtent3D srcSize, VkExtent3D dstSize)
{
    VkImageBlit2 blitRegion {};
    blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    blitRegion.pNext = nullptr;
    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = srcSize.depth;
    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = dstSize.depth;

    VkBlitImageInfo2 blitInfo {};
    blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitInfo.pNext = nullptr;
    blitInfo.srcImage = src;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.dstImage = dst;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;
    blitInfo.filter = VK_FILTER_LINEAR;

    vkCmdBlitImage2(command, &blitInfo);
}

void Application::init()
{
    Logger::init();

    m_Window.init();
    initVulkan();

    createSwapchain();
    createDrawImages();

    createCommandPools();

    createSyncStructures();

    createDescriptorPool();
    createDescriptors();

    setupSlang();

    createPipelines();

    m_Window.subscribe(EventFamily::KEYBOARD,
        std::bind(&Application::handleKeyInput, *this, std::placeholders::_1));

    LOG_INFO("Initialised application");
}

void Application::start()
{
    while (!m_Window.shouldClose()) {
        m_Window.pollEvents();

        render();
        update();
    }
}

void Application::cleanup()
{
    vkDeviceWaitIdle(m_VkDevice);

    destroyPipelines();

    destroyDescriptorPool();

    destroySyncStructures();
    destroyCommandPools();

    destroyDrawImages();
    destroySwapchain();

    vmaDestroyAllocator(m_VmaAllocator);
    vkDestroyDevice(m_VkDevice, nullptr);
    vkDestroySurfaceKHR(m_VkInstance, m_VkSurface, nullptr);
    vkb::destroy_debug_utils_messenger(m_VkInstance, m_VkDebugMessenger);
    vkDestroyInstance(m_VkInstance, nullptr);

    m_Window.cleanup();

    LOG_INFO("Cleaned up");
}

void Application::initVulkan()
{
    LOG_INFO("Init Vulkan");
    vkb::InstanceBuilder builder;
    auto builderRet = builder.set_app_name("Voxel Raymarcher")
                          .request_validation_layers(true)
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

    VkPhysicalDeviceVulkan12Features features12 {};
    features12.bufferDeviceAddress = true;

    VkPhysicalDeviceVulkan11Features features11 {};
    VkPhysicalDeviceFeatures features {};

    vkb::PhysicalDeviceSelector selector { vkbInst };
    auto vkbDeviceSelector = selector.set_minimum_version(1, 4)
                                 .set_required_features_14(features14)
                                 .set_required_features_13(features13)
                                 .set_required_features_12(features12)
                                 .set_required_features_11(features11)
                                 .set_required_features(features)
                                 .set_surface(m_VkSurface)
                                 .select();

    if (!vkbDeviceSelector.has_value()) {
        LOG_CRITICAL(
            "{}: {}", vkbDeviceSelector.error().value(), vkbDeviceSelector.error().message());
        exit(-1);
    }

    vkb::PhysicalDevice vkbPhysicalDevice = vkbDeviceSelector.value();
    vkb::DeviceBuilder deviceBuilder { vkbPhysicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

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

    LOG_INFO("Created swapchain");
}

void Application::destroySwapchain()
{
    vkDestroySwapchainKHR(m_VkDevice, m_VkSwapchain, nullptr);

    for (size_t i = 0; i < m_VkSwapchainImageViews.size(); i++) {
        vkDestroyImageView(m_VkDevice, m_VkSwapchainImageViews[i], nullptr);
    }

    LOG_INFO("Destroyed swapchain");
}

void Application::createDrawImages()
{
    VkExtent3D extent = { m_Window.getWindowSize().x, m_Window.getWindowSize().y, 1 };
    VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageCreateInfo imageCI {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.flags = 0;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent = extent;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocationCI {};
    allocationCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    allocationCI.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VkImageViewCreateInfo imageViewCI {};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.pNext = nullptr;
    imageViewCI.flags = 0;
    imageViewCI.image = VK_NULL_HANDLE;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = format;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].drawImage.format = format;
        m_PerFrameData[i].drawImage.extent = extent;

        VK_CHECK(vmaCreateImage(m_VmaAllocator, &imageCI, &allocationCI,
                     &m_PerFrameData[i].drawImage.image, &m_PerFrameData[i].drawImage.allocation,
                     nullptr),
            "Failed to allocate draw image");

        imageViewCI.image = m_PerFrameData[i].drawImage.image;
        VK_CHECK(
            vkCreateImageView(m_VkDevice, &imageViewCI, nullptr, &m_PerFrameData[i].drawImage.view),
            "Failed to create image view");
    }

    LOG_INFO("Created draw images");
}

void Application::destroyDrawImages()
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyImageView(m_VkDevice, m_PerFrameData[i].drawImage.view, nullptr);
        vmaDestroyImage(m_VmaAllocator, m_PerFrameData[i].drawImage.image,
            m_PerFrameData[i].drawImage.allocation);
    }

    LOG_INFO("Destroyed draw images");
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

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateCommandPool(
                     m_VkDevice, &commandPoolCI, nullptr, &m_PerFrameData[i].commandPool),
            "Failed to create command pool");

        commandBufferAI.commandPool = m_PerFrameData[i].commandPool;
        VK_CHECK(vkAllocateCommandBuffers(
                     m_VkDevice, &commandBufferAI, &m_PerFrameData[i].commandBuffer),
            "Failed to allocate command buffer");
    }

    LOG_INFO("Created command pools");
}

void Application::destroyCommandPools()
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyCommandPool(m_VkDevice, m_PerFrameData[i].commandPool, nullptr);
    }

    LOG_INFO("Destroyed command pools");
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
    }

    VkSemaphoreCreateInfo semaphoreCI {};
    semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCI.pNext = nullptr;
    semaphoreCI.flags = 0;

    m_RenderSemaphores.resize(m_VkSwapchainImages.size());
    m_SwapchainSemaphores.resize(m_VkSwapchainImages.size());

    for (size_t i = 0; i < m_VkSwapchainImages.size(); i++) {
        VK_CHECK(vkCreateSemaphore(m_VkDevice, &semaphoreCI, nullptr, &m_RenderSemaphores[i]),
            "Failed to create render semaphore");
        VK_CHECK(vkCreateSemaphore(m_VkDevice, &semaphoreCI, nullptr, &m_SwapchainSemaphores[i]),
            "Failed to create swapchain semaphore");
    }

    LOG_INFO("Created sync structures");
}

void Application::destroySyncStructures()
{
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroyFence(m_VkDevice, m_PerFrameData[i].fence, nullptr);
    }

    for (size_t i = 0; i < m_VkSwapchainImages.size(); i++) {
        vkDestroySemaphore(m_VkDevice, m_RenderSemaphores[i], nullptr);
        vkDestroySemaphore(m_VkDevice, m_SwapchainSemaphores[i], nullptr);
    }

    LOG_INFO("Destroyed sync structures");
}

void Application::createDescriptorPool()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {
         .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
         .descriptorCount = FRAMES_IN_FLIGHT,
         },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCI {};
    descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCI.pNext = nullptr;
    descriptorPoolCI.flags = 0;
    descriptorPoolCI.maxSets = FRAMES_IN_FLIGHT,
    descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCI.pPoolSizes = poolSizes.data();

    VK_CHECK(vkCreateDescriptorPool(m_VkDevice, &descriptorPoolCI, nullptr, &m_VkDescriptorPool),
        "Failed to create descriptor pool");

    LOG_INFO("Created descriptor pool");
}

void Application::createDescriptors()
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
                 m_VkDevice, &descriptorSetLayoutCI, nullptr, &m_ComputeDescriptorSetLayout),
        "Failed to create compute descriptor set layout");

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        descriptorSetLayouts.push_back(m_ComputeDescriptorSetLayout);

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
        imageInfo.imageView = m_PerFrameData[i].drawImage.view;
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
             }
        };

        vkUpdateDescriptorSets(m_VkDevice, writeSets.size(), writeSets.data(), 0, nullptr);
    }

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        m_PerFrameData[i].drawImageDescriptorSet = descriptorSets[i];
    }

    LOG_INFO("Created descriptors");
}

void Application::destroyDescriptorPool()
{
    vkDestroyDescriptorSetLayout(m_VkDevice, m_ComputeDescriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(m_VkDevice, m_VkDescriptorPool, nullptr);

    LOG_INFO("Destroyed descriptor pool");
}

void Application::setupSlang()
{
    slang::SessionDesc sessionDesc {};

    slang::createGlobalSession(m_GlobalSession.writeRef());

    slang::TargetDesc targetDesc = {};
    targetDesc.format = SLANG_SPIRV;
    targetDesc.profile = m_GlobalSession->findProfile("spirv_1_5");

    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    std::array<slang::CompilerOptionEntry, 1> options = {
        { slang::CompilerOptionName::EmitSpirvDirectly,
         { slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr } }
    };
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = options.size();

    const char* searchPaths[] = { "res/shaders/" };
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 1;

    m_GlobalSession->createSession(sessionDesc, m_Session.writeRef());

    LOG_INFO("Setup slang session");
}

void Application::createPipelines()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI {};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.pNext = nullptr;
    pipelineLayoutCI.flags = 0;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &m_ComputeDescriptorSetLayout;
    pipelineLayoutCI.pushConstantRangeCount = 0;
    pipelineLayoutCI.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(m_VkDevice, &pipelineLayoutCI, nullptr, &m_VkPipelineLayout),
        "Failed to create pipeline layout");

    VkShaderModule shaderModule = createShaderModule(m_VkDevice, "basic_compute", m_Session);

    VkPipelineShaderStageCreateInfo shaderStageCI {};
    shaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCI.pNext = nullptr;
    shaderStageCI.flags = 0;
    shaderStageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCI.module = shaderModule;
    shaderStageCI.pName = "main";
    shaderStageCI.pSpecializationInfo = nullptr;

    VkComputePipelineCreateInfo pipelineCI {};
    pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCI.pNext = nullptr;
    pipelineCI.flags = 0;
    pipelineCI.stage = shaderStageCI;
    pipelineCI.layout = m_VkPipelineLayout;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = 0;

    VK_CHECK(vkCreateComputePipelines(
                 m_VkDevice, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_VkPipeline),
        "Failed to create compute pipeline");

    vkDestroyShaderModule(m_VkDevice, shaderModule, nullptr);

    LOG_INFO("Created pipelines");
}

void Application::destroyPipelines()
{
    vkDestroyPipeline(m_VkDevice, m_VkPipeline, nullptr);
    vkDestroyPipelineLayout(m_VkDevice, m_VkPipelineLayout, nullptr);

    LOG_INFO("Destroyed pipelines");
}

void Application::render()
{
    PerFrameData& currentFrame = m_PerFrameData[m_CurrentFrameIndex];

    uint64_t timeout = 1e9;

    VK_CHECK(vkWaitForFences(m_VkDevice, 1, &currentFrame.fence, true, timeout), "Fence");

    VK_CHECK(vkResetFences(m_VkDevice, 1, &currentFrame.fence), "Reset fence");

    uint32_t swapchainImageIndex = 0;
    vkAcquireNextImageKHR(m_VkDevice, m_VkSwapchain, timeout,
        m_SwapchainSemaphores[m_CurrentSemaphore], nullptr, &swapchainImageIndex);

    VkCommandBuffer commandBuffer = currentFrame.commandBuffer;
    VK_CHECK(vkResetCommandBuffer(commandBuffer, 0), "Reset command Buffer");

    VkCommandBufferBeginInfo commandBufferBI {};
    commandBufferBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBI.pNext = nullptr;
    commandBufferBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    commandBufferBI.pInheritanceInfo = nullptr;

    {
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBI), "Begin command buffer");

        transitionImage(commandBuffer, m_VkSwapchainImages[swapchainImageIndex],
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        transitionImage(commandBuffer, currentFrame.drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_VkPipelineLayout,
            0, 1, &currentFrame.drawImageDescriptorSet, 0, nullptr);

        vkCmdDispatch(commandBuffer, currentFrame.drawImage.extent.width / 8,
            currentFrame.drawImage.extent.height / 8, 1);

        transitionImage(commandBuffer, currentFrame.drawImage.image, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        copyImageToImage(commandBuffer, currentFrame.drawImage.image,
            m_VkSwapchainImages[swapchainImageIndex], currentFrame.drawImage.extent,
            m_VkSwapchainImageExtent);

        transitionImage(commandBuffer, m_VkSwapchainImages[swapchainImageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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
        waitSI.semaphore = m_SwapchainSemaphores[m_CurrentSemaphore];
        waitSI.value = 1;
        waitSI.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
        waitSI.deviceIndex = 0;

        VkSemaphoreSubmitInfo signalSI {};
        signalSI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signalSI.pNext = nullptr;
        signalSI.semaphore = m_RenderSemaphores[m_CurrentSemaphore];
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
        presentInfo.pWaitSemaphores = &m_RenderSemaphores[m_CurrentSemaphore];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_VkSwapchain;
        presentInfo.pImageIndices = &swapchainImageIndex;
        presentInfo.pResults = nullptr;
        vkQueuePresentKHR(m_GraphicsQueue.queue, &presentInfo);
    }

    m_Window.swapBuffers();
}

void Application::update()
{
    m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % FRAMES_IN_FLIGHT;
    m_CurrentSemaphore = (m_CurrentSemaphore + 1) % m_VkSwapchainImages.size();
}

void Application::handleKeyInput(const Event& event)
{
    const KeyboardEvent& kEvent = static_cast<const KeyboardEvent&>(event);

    if (kEvent.type() == KeyboardEventType::PRESS) {
        LOG_INFO("Press key");
    }
}
