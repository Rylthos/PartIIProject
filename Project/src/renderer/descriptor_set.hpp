#pragma once

#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "buffer.hpp"
#include "image.hpp"

class DescriptorSetGenerator {
    struct BufferDescriptor {
        Buffer& buffer;
        uint32_t binding;
        uint32_t offset;
        VkDescriptorType types;
    };

    struct ImageDescriptor {
        Image& image;
        VkImageLayout layout;
        uint32_t binding;
        VkDescriptorType types;
    };

  public:
    static DescriptorSetGenerator start(
        VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);

    DescriptorSetGenerator& addBufferDescriptor(
        uint32_t binding, Buffer& buffer, uint32_t offset = 0);
    DescriptorSetGenerator& addImageDescriptor(
        uint32_t binding, Image& image, VkImageLayout layout);

    DescriptorSetGenerator& setDebugName(const char* name);

    VkDescriptorSet build();

  private:
    DescriptorSetGenerator(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
        : m_Device(device), m_DescriptorPool(pool), m_Layout(layout)
    {
    }

  private:
    VkDevice m_Device;
    VkDescriptorPool m_DescriptorPool;
    VkDescriptorSetLayout m_Layout;

    std::vector<BufferDescriptor> m_Buffers;
    std::vector<ImageDescriptor> m_Images;

    std::optional<std::string> m_DebugName;
};
