#pragma once

#include <initializer_list>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_core.h>

class PipelineLayoutGenerator {
  public:
    static PipelineLayoutGenerator start(VkDevice device);

    PipelineLayoutGenerator& addDescriptorLayout(VkDescriptorSetLayout layout);
    PipelineLayoutGenerator& addDescriptorLayouts(
        std::initializer_list<VkDescriptorSetLayout> layouts);

    PipelineLayoutGenerator& addPushConstant(
        VkShaderStageFlags flags, uint32_t offset, uint32_t size);

    PipelineLayoutGenerator& setDebugName(const char* name);

    VkPipelineLayout build();

  private:
    PipelineLayoutGenerator(VkDevice device) : m_Device(device) { }

  private:
    VkDevice m_Device;
    std::vector<VkDescriptorSetLayout> m_Descriptors;
    std::vector<VkPushConstantRange> m_PushConstants;

    std::optional<std::string> m_DebugName;
};
