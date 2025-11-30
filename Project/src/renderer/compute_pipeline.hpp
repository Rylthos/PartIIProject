#pragma once

#include <optional>
#include <string>
#include <vulkan/vulkan.h>

class ComputePipelineGenerator {
  public:
    static ComputePipelineGenerator start(VkDevice device, VkPipelineLayout layout);

    ComputePipelineGenerator& setShader(const char* shader);

    ComputePipelineGenerator& setDebugName(const char* name);

    VkPipeline build();

  private:
    ComputePipelineGenerator(VkDevice device, VkPipelineLayout layout)
        : m_Device(device), m_Layout(layout) { };

  private:
    VkDevice m_Device;
    VkPipelineLayout m_Layout;

    VkPipelineShaderStageCreateInfo m_ShaderStage;

    std::optional<std::string> m_DebugName;
};
