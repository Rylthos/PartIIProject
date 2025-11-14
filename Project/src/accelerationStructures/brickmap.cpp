#include "brickmap.hpp"

#include "../debug_utils.hpp"
#include "../shader_manager.hpp"
#include "acceleration_structure.hpp"

#include <vulkan/vulkan_core.h>

BrickmapAS::BrickmapAS() { }
BrickmapAS::~BrickmapAS()
{
    destroyRenderPipeline();
    destroyDescriptorLayout();
    freeDescriptorSet();
    destroyDescriptorLayout();
    freeBuffers();

    ShaderManager::getInstance()->removeModule("brickmap_AS");
}

void BrickmapAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();
    createBuffers();
    createDescriptorSet();

    createRenderPipelineLayout();
    createRenderPipeline();
}

void BrickmapAS::fromLoader(std::unique_ptr<Loader>&& loader) { }
void BrickmapAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    beginCmdDebugLabel(cmd, "Brickmap AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        renderSet,
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    endCmdDebugLabel(cmd);
}

void BrickmapAS::update(float dt) { }

void BrickmapAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("brickmap_AS"); }

void BrickmapAS::createDescriptorLayout() { }

void BrickmapAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void BrickmapAS::createBuffers() { }

void BrickmapAS::freeBuffers() { }

void BrickmapAS::createDescriptorSet() { }

void BrickmapAS::freeDescriptorSet()
{
    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_BufferSet);
}

void BrickmapAS::createRenderPipelineLayout() { }

void BrickmapAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
}

void BrickmapAS::createRenderPipeline() { }

void BrickmapAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}
