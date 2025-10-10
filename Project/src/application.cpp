#include "application.hpp"

#include "VkBootstrap.h"

#include <iostream>

void Application::init()
{
    Logger::init();

    m_Window.init();
    initVulkan();

    LOG_INFO("Initialised application");
}

void Application::start() { std::cout << "Hello, World!\n"; }

void Application::cleanup()
{
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

