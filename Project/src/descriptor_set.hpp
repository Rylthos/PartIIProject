#pragma once

#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "buffer.hpp"

class DescriptorSetGenerator {
    struct BufferDescriptor {
        Buffer& buffer;
        uint32_t binding;
        uint32_t offset;
        VkDescriptorType types;
    };

  public:
    static DescriptorSetGenerator start(
        VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout);

    DescriptorSetGenerator& addBufferDescriptor(
        uint32_t binding, Buffer& buffer, uint32_t offset = 0);

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

    std::optional<std::string> m_DebugName;
};
