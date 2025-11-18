#include "compute_pipeline.hpp"

#include "debug_utils.hpp"
#include "logger.hpp"
#include "shader_manager.hpp"

ComputePipelineGenerator ComputePipelineGenerator::start(VkDevice device, VkPipelineLayout layout)
{
    return ComputePipelineGenerator(device, layout);
}

ComputePipelineGenerator& ComputePipelineGenerator::setShader(const char* shader)
{
    VkShaderModule module = ShaderManager::getInstance()->getShaderModule(shader);

    m_ShaderStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    return *this;
}

ComputePipelineGenerator& ComputePipelineGenerator::setDebugName(const char* name)
{
    m_DebugName = std::string(name);

    return *this;
}

VkPipeline ComputePipelineGenerator::build()
{
    VkComputePipelineCreateInfo pipelineCI {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = m_ShaderStage,
        .layout = m_Layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline),
        "Failed to create contree render pipeline");

    if (m_DebugName.has_value()) {
        Debug::setDebugName(
            m_Device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)pipeline, m_DebugName.value().c_str());
    }

    return pipeline;
}
