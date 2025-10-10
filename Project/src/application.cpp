#include "application.hpp"

#include "VkBootstrap.h"

#include <iostream>

void Application::init()
{
    Logger::init();

    m_Window.init();
    initVulkan();

    createSwapchain();
    createDrawImages();
    LOG_INFO("Initialised application");
}

void Application::start() { std::cout << "Hello, World!\n"; }

void Application::cleanup()
{
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
              .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
              .build()
              .value();

    m_VkSwapchain = vkbSwapchain.swapchain;
    m_VkSwapchainImageExtent = vkbSwapchain.extent;
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
    m_DrawImage.extent = { m_Window.getWindowSize().x, m_Window.getWindowSize().y, 1 };
    m_DrawImage.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    VkImageCreateInfo imageCI {};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.flags = 0;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = m_DrawImage.format;
    imageCI.extent = m_DrawImage.extent;
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
    VK_CHECK(vmaCreateImage(m_VmaAllocator, &imageCI, &allocationCI, &m_DrawImage.image,
                 &m_DrawImage.allocation, nullptr),
        "Failed to allocate draw image");

    VkImageViewCreateInfo imageViewCI {};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.pNext = nullptr;
    imageViewCI.flags = 0;
    imageViewCI.image = m_DrawImage.image;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.format = m_DrawImage.format;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(m_VkDevice, &imageViewCI, nullptr, &m_DrawImage.view),
        "Failed to create image view");

    LOG_INFO("Created draw images");
}

void Application::destroyDrawImages()
{
    vkDestroyImageView(m_VkDevice, m_DrawImage.view, nullptr);
    vmaDestroyImage(m_VmaAllocator, m_DrawImage.image, nullptr);

    LOG_INFO("Destroyed draw images");
}
