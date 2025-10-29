#include "octree.hpp"
#include <memory>
#include <unistd.h>
#include <variant>
#include <vulkan/vulkan_core.h>

#include "../debug_utils.hpp"
#include "../logger.hpp"
#include "../shader_manager.hpp"
#include "accelerationStructure.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec3 cameraForward;
    alignas(16) glm::vec3 cameraRight;
    alignas(16) glm::vec3 cameraUp;
};

OctreeNode::OctreeNode(uint8_t childMask, uint16_t offset)
{
    m_CurrentType = NodeType {
        .flags = OCTREE_FLAG_EMPTY,
        .childMask = childMask,
        .offset = offset,
    };
}

OctreeNode::OctreeNode(uint8_t r, uint8_t g, uint8_t b)
{
    m_CurrentType = LeafType {
        .flags = OCTREE_FLAG_SOLID,
        .r = r,
        .g = g,
        .b = b,
    };
}

uint32_t OctreeNode::getData()
{
    if (const NodeType* node = std::get_if<NodeType>(&m_CurrentType)) {
        uint32_t flags = ((uint32_t)node->flags) << 24;
        uint32_t childMask = ((uint32_t)node->childMask) << 16;
        uint32_t offset = ((uint32_t)node->offset) << 0;
        return flags | childMask | offset;
    } else if (const LeafType* leaf = std::get_if<LeafType>(&m_CurrentType)) {
        uint32_t flags = ((uint32_t)leaf->flags) << 24;
        uint32_t r = ((uint32_t)leaf->r) << 16;
        uint32_t g = ((uint32_t)leaf->g) << 8;
        uint32_t b = ((uint32_t)leaf->b) << 0;
        return flags | r | g | b;
    } else {
        assert(false && "Not possible");
    }
}

OctreeAS::OctreeAS() { }

OctreeAS::~OctreeAS()
{
    freeDescriptorSet();
    m_StagingBuffer.cleanup();
    m_OctreeBuffer.cleanup();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("octree_AS");
}

void OctreeAS::init(ASStructInfo info)
{
    m_Info = info;

    // Single Node
    // m_Nodes = {
    //     OctreeNode(0, 255, 255),
    // };

    // Descending corner
    // for (size_t i = 0; i < 10; i++) {
    //     m_Nodes.emplace_back(0xFF, 0x1);
    //     m_Nodes.emplace_back(255, 255, 255);
    //     m_Nodes.emplace_back(255, 0, 255);
    //     m_Nodes.emplace_back(255, 255, 0);
    //     m_Nodes.emplace_back(0, 255, 255);
    //     m_Nodes.emplace_back(0, 0, 255);
    //     m_Nodes.emplace_back(0, 255, 0);
    //     m_Nodes.emplace_back(255, 0, 0);
    // }
    //
    // m_Nodes.emplace_back(0, 0, 0);

    // Alternative Corners
    // m_Nodes = {
    //     OctreeNode(0x69, 0x1),
    //     OctreeNode(255, 255, 255),
    //     OctreeNode(0, 0, 255),
    //     OctreeNode(0, 255, 0),
    //     OctreeNode(255, 0, 0),
    // };

    // Alternative corners 2 deep
    // m_Nodes = {
    //     OctreeNode(0x69, 0x1),
    //     OctreeNode(0x69, 0x4),
    //     OctreeNode(0x69, 0x7),
    //     OctreeNode(0x69, 0xA),
    //     OctreeNode(0x69, 0xD),
    //
    //     OctreeNode(255, 255, 255),
    //     OctreeNode(0, 0, 255),
    //     OctreeNode(0, 255, 0),
    //     OctreeNode(255, 0, 0),
    //
    //     OctreeNode(255, 255, 255),
    //     OctreeNode(0, 0, 255),
    //     OctreeNode(0, 255, 0),
    //     OctreeNode(255, 0, 0),
    //
    //     OctreeNode(255, 255, 255),
    //     OctreeNode(0, 0, 255),
    //     OctreeNode(0, 255, 0),
    //     OctreeNode(255, 0, 0),
    //
    //     OctreeNode(255, 255, 255),
    //     OctreeNode(0, 0, 255),
    //     OctreeNode(0, 255, 0),
    //     OctreeNode(255, 0, 0),
    // };

    // Alternative corners 3 deep
    m_Nodes = {
        OctreeNode(0x69, 1),
        OctreeNode(0x69, 4),
        OctreeNode(0x69, 7),
        OctreeNode(0x69, 10),
        OctreeNode(0x69, 13),

        OctreeNode(0x69, 16),
        OctreeNode(0x69, 19),
        OctreeNode(0x69, 22),
        OctreeNode(0x69, 25),

        OctreeNode(0x69, 28),
        OctreeNode(0x69, 31),
        OctreeNode(0x69, 34),
        OctreeNode(0x69, 37),

        OctreeNode(0x69, 40),
        OctreeNode(0x69, 43),
        OctreeNode(0x69, 46),
        OctreeNode(0x69, 49),

        OctreeNode(0x69, 52),
        OctreeNode(0x69, 55),
        OctreeNode(0x69, 58),
        OctreeNode(0x69, 61),
    };
    for (size_t i = 0; i < 16; i++) {
        m_Nodes.emplace_back(255, 255, 255);
        m_Nodes.emplace_back(0, 0, 255);
        m_Nodes.emplace_back(0, 255, 0);
        m_Nodes.emplace_back(255, 0, 0);
    }

    createDescriptorLayout();
    createBuffers();
    createDescriptorSet();

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

    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .cameraForward = camera.getForwardVector(),
        .cameraRight = camera.getRightVector(),
        .cameraUp = camera.getUpVector(),
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        drawImageSet,
        m_BufferSet,
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstant);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    endCmdDebugLabel(cmd);
}

void OctreeAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("octree_AS"); }

void OctreeAS::createDescriptorLayout()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
         .binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = nullptr,
         }
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

void OctreeAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(m_Info.device, m_BufferSetLayout, nullptr);
}

void OctreeAS::createBuffers()
{
    VkDeviceSize size = sizeof(uint32_t) * m_Nodes.size();
    m_OctreeBuffer.init(m_Info.device, m_Info.allocator, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_OctreeBuffer.setName("Octree node buffer");

    m_StagingBuffer.init(m_Info.device, m_Info.allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
    m_StagingBuffer.setName("Staging buffer");

    uint32_t* data = reinterpret_cast<uint32_t*>(m_StagingBuffer.mapMemory());
    for (size_t i = 0; i < m_Nodes.size(); i++) {
        data[i] = m_Nodes[i].getData();
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

    m_StagingBuffer.copyToBuffer(cmd, m_OctreeBuffer, size, 0, 0);

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

void OctreeAS::createDescriptorSet()
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
        .buffer = m_OctreeBuffer.getBuffer(),
        .offset = 0,
        .range = m_OctreeBuffer.getSize(),
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

void OctreeAS::freeDescriptorSet()
{
    vkFreeDescriptorSets(m_Info.device, m_Info.descriptorPool, 1, &m_BufferSet);
}

void OctreeAS::createRenderPipelineLayout()
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
