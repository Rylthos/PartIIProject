#include "octree.hpp"
#include <vulkan/vulkan_core.h>

#include "../debug_utils.hpp"
#include "../logger.hpp"
#include "../shader_manager.hpp"

OctreeAS::OctreeAS() { }

OctreeAS::~OctreeAS()
{
    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("octree_AS");
}

void OctreeAS::init(ASStructInfo info)
{
    m_Info = info;

    createRenderPipelineLayout();
    ShaderManager::getInstance()->addModule("octree_AS",
        std::bind(&OctreeAS::createRenderPipeline, this),
        std::bind(&OctreeAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void OctreeAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
{
    beginCmdDebugLabel(cmd, "Octree AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        drawImageSet,
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    // vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
    //     sizeof(PushConstants), &pushConstant);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    endCmdDebugLabel(cmd);
}

void OctreeAS::updateShaders() { }

void OctreeAS::createRenderPipelineLayout()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts = { m_Info.drawImageDescriptorLayout };

    VkPipelineLayoutCreateInfo layoutCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };

    VK_CHECK(vkCreatePipelineLayout(m_Info.device, &layoutCI, nullptr, &m_RenderPipelineLayout),
        "Failed to create render pipeline layout");

    setDebugName(m_Info.device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_RenderPipelineLayout,
        "Octree render pipeline layout");
}

void OctreeAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(m_Info.device, m_RenderPipelineLayout, nullptr);
}

void OctreeAS::createRenderPipeline()
{
    VkShaderModule shaderModule = ShaderManager::getInstance()->getShaderModule("octree_AS");

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
        "Failed to create octree render pipeline");

    setDebugName(m_Info.device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_RenderPipeline,
        "Octree render pipeline");
}

void OctreeAS::destroyRenderPipeline()
{
    vkDestroyPipeline(m_Info.device, m_RenderPipeline, nullptr);
}
