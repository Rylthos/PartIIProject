#include "grid.hpp"

#include "accelerationStructure.hpp"

#include <vulkan/vulkan_core.h>

#include "../debug_utils.hpp"
#include "../logger.hpp"
#include "../shader_manager.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec3 cameraForward;
    alignas(16) glm::vec3 cameraRight;
    alignas(16) glm::vec3 cameraUp;
};

GridAS::GridAS() { }

GridAS::~GridAS()
{
    destroyRenderPipeline();
    destroyRenderPipelineLayout();
}

void GridAS::init(ASStructInfo info)
{
    m_Info = info;
    // Setup buffer

    createRenderPipelineLayout();
    ShaderManager::getInstance()->addModule("Grid_AS",
        std::bind(&GridAS::createRenderPipeline, this),
        std::bind(&GridAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void GridAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
{
    beginCmdDebugLabel(cmd, "Grid AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    PushConstants pushConstant {
        .cameraPosition = camera.getPosition(),
        .cameraForward = camera.getForwardVector(),
        .cameraRight = camera.getRightVector(),
        .cameraUp = camera.getUpVector(),
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0, 1,
        &drawImageSet, 0, nullptr);
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstant);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    endCmdDebugLabel(cmd);
}

void GridAS::createRenderPipelineLayout()
{
    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutCI {};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.pNext = nullptr;
    layoutCI.flags = 0;
    layoutCI.setLayoutCount = 1;
    layoutCI.pSetLayouts = &m_Info.drawImageDescriptorLayout;
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(m_Info.device, &layoutCI, nullptr, &m_RenderPipelineLayout),
        "Failed to create render pipeline layout");

    setDebugName(m_Info.device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_RenderPipelineLayout,
        "Grid render pipeline layout");
}

void GridAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(m_Info.device, m_RenderPipelineLayout, nullptr);
}

void GridAS::createRenderPipeline()
{
    VkShaderModule shaderModule = ShaderManager::getInstance()->getShaderModule("Grid_AS");

    VkPipelineShaderStageCreateInfo shaderStageCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    VkComputePipelineCreateInfo pipelineCI {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shaderStageCI,
        .layout = m_RenderPipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VK_CHECK(vkCreateComputePipelines(
                 m_Info.device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_RenderPipeline),
        "Failed to create Grid render pipeline");

    setDebugName(
        m_Info.device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_RenderPipeline, "Grid render pipeline");
}

void GridAS::destroyRenderPipeline()
{
    vkDestroyPipeline(m_Info.device, m_RenderPipeline, nullptr);
}
