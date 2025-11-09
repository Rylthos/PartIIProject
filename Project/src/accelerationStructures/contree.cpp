#include "contree.hpp"
#include <vulkan/vulkan_core.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/matrix_operation.hpp>

#include "../debug_utils.hpp"
#include "../shader_manager.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec3 cameraForward;
    alignas(16) glm::vec3 cameraRight;
    alignas(16) glm::vec3 cameraUp;
    alignas(16) glm::mat4 contreeWorld;
    alignas(16) glm::mat4 contreeWorldInverse;
    alignas(16) glm::mat4 contreeScaleInverse;
};

ContreeNode::ContreeNode(uint64_t childMask, uint32_t offset, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t colour = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    m_CurrentType = NodeType {
        .flags = CONTREE_FLAG_EMPTY,
        .colour = colour,
        .offset = offset,
        .childMask = childMask,
    };
}

ContreeNode::ContreeNode(float r, float g, float b)
{
    uint32_t rI = (uint32_t)(std::clamp(r, 0.f, 1.f) * (float)0xFFFF);
    uint32_t gI = (uint32_t)(std::clamp(g, 0.f, 1.f) * (float)0xFFFF);
    uint32_t bI = (uint32_t)(std::clamp(b, 0.f, 1.f) * (float)0xFFFF);
    m_CurrentType = LeafType {
        .flags = CONTREE_FLAG_SOLID,
        .r = rI,
        .g = gI,
        .b = bI,
    };
}

std::array<uint64_t, 2> ContreeNode::getData()
{
    std::array<uint64_t, 2> data;
    if (const NodeType* node = std::get_if<NodeType>(&m_CurrentType)) {
        uint64_t flags = ((uint64_t)node->flags) << 56;
        uint64_t colour = ((uint64_t)node->colour) << 32;
        uint64_t offset = node->offset;
        uint64_t childMask = node->childMask;
        data[0] = flags | colour | offset;
        data[1] = childMask;
    } else if (const LeafType* leaf = std::get_if<LeafType>(&m_CurrentType)) {
        uint64_t flags = ((uint64_t)leaf->flags) << 56;
        uint64_t r = (uint64_t)leaf->r;
        uint64_t g = (uint64_t)leaf->g << 32;
        uint64_t b = (uint64_t)leaf->b;

        data[0] = flags | r;
        data[1] = g | b;
    }

    return data;
}

ContreeAS::ContreeAS() { }
ContreeAS::~ContreeAS()
{
    freeDescriptorSet();
    destroyBuffers();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("contree_AS");
}

void ContreeAS::init(ASStructInfo info)
{
    m_Info = info;
    createDescriptorLayout();
    createRenderPipelineLayout();

    ShaderManager::getInstance()->addModule("contree_AS",
        std::bind(&ContreeAS::createRenderPipeline, this),
        std::bind(&ContreeAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void ContreeAS::fromLoader(Loader& loader)
{
    freeDescriptorSet();
    destroyBuffers();

    m_Nodes = {
        ContreeNode(0xFFFFFFFFFFFFFFFF, 0x1, 255, 255, 255),
    };

    uint32_t offset = 0;
    for (size_t i = 0; i < 64; i++) {
        m_Nodes.push_back(ContreeNode(0xFFFFFFFFFFFFFFFF, (64 - i) + offset, 255, 255, 255));
        offset += 64;
    }

    uint32_t sidelength = pow(4, 2);
    uint32_t voxels = pow(sidelength, 3);
    for (size_t j = 0; j < 64; j++) {
        glm::ivec3 offset = {
            j % 4,
            (j / 4) % 4,
            (j / 16) % 4,
        };
        offset *= 4;
        for (size_t i = 0; i < 64; i++) {
            glm::ivec3 index {
                i % 4,
                (i / 4) % 4,
                (i / 16) % 4,
            };
            glm::vec3 colour = (glm::vec3(offset + index)) / glm::vec3(sidelength);
            m_Nodes.push_back(ContreeNode(colour.x, colour.y, colour.z));
        }
    }

    createBuffers();
    createDescriptorSet();
}

void ContreeAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
{
    beginCmdDebugLabel(cmd, "Contree AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);

    std::vector<VkDescriptorSet> descriptorSets = {
        drawImageSet,
        m_BufferSet,
    };

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    glm::vec3 scale = glm::vec3(10);

    glm::mat4 contreeWorld = glm::mat4(1);
    contreeWorld = glm::scale(contreeWorld, scale);
    contreeWorld = glm::translate(contreeWorld, glm::vec3(-1));
    glm::mat4 contreeWorldInverse = glm::inverse(contreeWorld);

    glm::mat4 contreeScaleInverse = glm::inverse(glm::scale(glm::mat4(1), scale));

    PushConstants pushConstants = {
        .cameraPosition = camera.getPosition(),
        .cameraForward = camera.getForwardVector(),
        .cameraRight = camera.getRightVector(),
        .cameraUp = camera.getUpVector(),
        .contreeWorld = contreeWorld,
        .contreeWorldInverse = contreeWorldInverse,
        .contreeScaleInverse = contreeScaleInverse,
    };
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstants);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    endCmdDebugLabel(cmd);
}

void ContreeAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("contree_AS"); }

uint64_t ContreeAS::getMemoryUsage() { return m_ContreeBuffer.getSize(); }
uint64_t ContreeAS::getStoredVoxels() { return 0; }
uint64_t ContreeAS::getTotalVoxels() { return 0; }

void ContreeAS::createDescriptorLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
         .binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = nullptr,
         },
    };

    VkDescriptorSetLayoutCreateInfo setLayoutCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    vkCreateDescriptorSetLayout(m_Info.device, &setLayoutCI, nullptr, &m_BufferSetLayout);
}

void ContreeAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(m_Info.device, m_BufferSetLayout, nullptr);
}

void ContreeAS::createBuffers()
{
    VkDeviceSize size = sizeof(uint64_t) * 2 * m_Nodes.size();
    m_ContreeBuffer.init(m_Info.device, m_Info.allocator, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ContreeBuffer.setName("Contree node buffer");

    m_StagingBuffer.init(m_Info.device, m_Info.allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
    m_StagingBuffer.setName("Staging buffer");

    uint64_t* data = reinterpret_cast<uint64_t*>(m_StagingBuffer.mapMemory());
    for (size_t i = 0; i < m_Nodes.size(); i++) {
        const auto& node = m_Nodes[i].getData();
        data[i * 2] = node[0];
        data[i * 2 + 1] = node[1];
    }

    m_StagingBuffer.unmapMemory();

    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo commandBufferAI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_Info.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(vkAllocateCommandBuffers(m_Info.device, &commandBufferAI, &cmd),
        "Failed to allocate command buffer");

    VkCommandBufferBeginInfo commandBI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(cmd, &commandBI);

    m_StagingBuffer.copyToBuffer(cmd, m_ContreeBuffer, size, 0, 0);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    vkQueueSubmit(m_Info.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Info.graphicsQueue);

    vkFreeCommandBuffers(m_Info.device, m_Info.commandPool, 1, &cmd);
}

void ContreeAS::destroyBuffers()
{
    m_StagingBuffer.cleanup();
    m_ContreeBuffer.cleanup();
}

void ContreeAS::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo setAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_Info.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_BufferSetLayout,
    };
    vkAllocateDescriptorSets(m_Info.device, &setAI, &m_BufferSet);

    VkDescriptorBufferInfo octreeBI {
        .buffer = m_ContreeBuffer.getBuffer(),
        .offset = 0,
        .range = m_ContreeBuffer.getSize(),
    };

    std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
        {
         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = nullptr,
         .dstSet = m_BufferSet,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = nullptr,
         .pBufferInfo = &octreeBI,
         .pTexelBufferView = nullptr,
         }
    };

    vkUpdateDescriptorSets(
        m_Info.device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void ContreeAS::freeDescriptorSet()
{
    vkFreeDescriptorSets(m_Info.device, m_Info.descriptorPool, 1, &m_BufferSet);
}

void ContreeAS::createRenderPipelineLayout()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts
        = { m_Info.drawImageDescriptorLayout, m_BufferSetLayout };

    std::vector<VkPushConstantRange> pushConstantRanges = {
        {
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .offset = 0,
         .size = sizeof(PushConstants),
         },
    };

    VkPipelineLayoutCreateInfo layoutCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size()),
        .pSetLayouts = descriptorSetLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),
    };

    VK_CHECK(vkCreatePipelineLayout(m_Info.device, &layoutCI, nullptr, &m_RenderPipelineLayout),
        "Failed to create render pipeline layout");

    setDebugName(m_Info.device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_RenderPipelineLayout,
        "Contree render pipeline layout");
}

void ContreeAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(m_Info.device, m_RenderPipelineLayout, nullptr);
}

void ContreeAS::createRenderPipeline()
{
    VkShaderModule shaderModule = ShaderManager::getInstance()->getShaderModule("contree_AS");

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
        "Failed to create contree render pipeline");

    setDebugName(m_Info.device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_RenderPipeline,
        "Contree render pipeline");
}

void ContreeAS::destroyRenderPipeline()
{
    vkDestroyPipeline(m_Info.device, m_RenderPipeline, nullptr);
}
