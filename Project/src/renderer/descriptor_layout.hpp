#pragma once

#include <optional>
#include <string>
#include <vulkan/vulkan.h>

#include <vector>

class DescriptorLayoutGenerator {
  public:
    static DescriptorLayoutGenerator start(VkDevice device);

    DescriptorLayoutGenerator& addBinding(VkDescriptorType type, VkShaderStageFlags flags,
        uint32_t binding, uint32_t descriptorCount);

    DescriptorLayoutGenerator& addStorageBufferBinding(VkShaderStageFlags flags, uint32_t binding);
    DescriptorLayoutGenerator& addStorageImageBinding(VkShaderStageFlags flags, uint32_t binding);

    DescriptorLayoutGenerator& setDebugName(const char* name);

    VkDescriptorSetLayout build();

  private:
    DescriptorLayoutGenerator(VkDevice device) : m_Device(device) { }

  private:
    VkDevice m_Device;

    std::vector<VkDescriptorSetLayoutBinding> m_Bindings;
    std::optional<std::string> m_DebugName;
};
