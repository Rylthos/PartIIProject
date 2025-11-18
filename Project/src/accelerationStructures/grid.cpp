#include "grid.hpp"

#include "acceleration_structure.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

#include "../debug_utils.hpp"
#include "../frame_commands.hpp"
#include "../logger.hpp"
#include "../shader_manager.hpp"
#include "spdlog/spdlog.h"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::uvec3 dimensions;
};

GridAS::GridAS() { }

GridAS::~GridAS()
{
    freeBuffers();

    freeDescriptorSets();
    destroyDescriptorLayouts();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("grid_AS");
}

void GridAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayouts();

    createRenderPipelineLayout();
    ShaderManager::getInstance()->addModule("grid_AS",
        std::bind(&GridAS::createRenderPipeline, this),
        std::bind(&GridAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void GridAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    freeBuffers();
    freeDescriptorSets();

    m_Dimensions = loader->getDimensions();

    m_Voxels.resize(m_Dimensions.x * m_Dimensions.y * m_Dimensions.z);
    for (size_t z = 0; z < m_Dimensions.z; z++) {
        for (size_t y = 0; y < m_Dimensions.y; y++) {
            for (size_t x = 0; x < m_Dimensions.x; x++) {
                size_t index = x + y * m_Dimensions.x + z * m_Dimensions.x * m_Dimensions.y;
                auto v = loader->getVoxel({ x, y, z });

                m_Voxels[index] = GridVoxel {
                    .visible = v.has_value(),
                    .colour = v.has_value() ? v.value().colour : glm::vec3 { 0, 0, 0 },
                };
            }
        }
    }

    createBuffer();
    createDescriptorSets();
}

void GridAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Grid AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    PushConstants pushConstant {
        .cameraPosition = camera.getPosition(),
        .dimensions = m_Dimensions,
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        renderSet,
        m_BufferSet,
    };
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstant);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    Debug::endCmdDebugLabel(cmd);
}

void GridAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("grid_AS"); }

void GridAS::createDescriptorLayouts()
{
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
         .binding = 0,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         },
        {
         .binding = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .descriptorCount = 1,
         .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .pImmutableSamplers = VK_NULL_HANDLE,
         }
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VK_CHECK(vkCreateDescriptorSetLayout(
                 p_Info.device, &descriptorSetLayoutCI, nullptr, &m_BufferSetLayout),
        "Failed to create buffer set layout");

    Debug::setDebugName(p_Info.device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
        (uint64_t)m_BufferSetLayout, "Grid buffer set layout");
}

void GridAS::destroyDescriptorLayouts()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void GridAS::createBuffer()
{
    VkDeviceSize occupancyBufferSize = std::ceil(m_Voxels.size() / 8.); // Convert to bytes
    VkDeviceSize colourBufferSize = sizeof(glm::vec3) * m_Voxels.size();

    m_OccupancyBuffer.init(p_Info.device, p_Info.allocator, occupancyBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_OccupancyBuffer.setDebugName("Grid occupancy buffer");

    m_ColourBuffer.init(p_Info.device, p_Info.allocator, colourBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ColourBuffer.setDebugName("Grid colour buffer");

    auto bufferIndex = FrameCommands::getInstance()->createStaging(
        occupancyBufferSize + colourBufferSize, [=, this](void* ptr) {
            uint8_t* dataOccupancy = (uint8_t*)ptr;
            float* dataColour = (float*)(dataOccupancy + occupancyBufferSize);

            size_t colourIndex = 0;
            uint16_t current_index = 0;
            uint8_t current_mask = 0;
            for (size_t i = 0; i < m_Voxels.size(); i++) {
                dataColour[colourIndex++] = m_Voxels[i].colour.x;
                dataColour[colourIndex++] = m_Voxels[i].colour.y;
                dataColour[colourIndex++] = m_Voxels[i].colour.z;

                if (i / 8 != current_index) {
                    dataOccupancy[current_index] = current_mask;
                    current_index = i / 8;
                    current_mask = 0;
                }

                current_mask |= (m_Voxels[i].visible & 1) << (7 - (i % 8));
            }
            dataOccupancy[current_index] = current_mask;
        });

    FrameCommands::getInstance()->stagingEval(
        bufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = occupancyBufferSize,
            };

            vkCmdCopyBuffer(cmd, buffer.buffer, m_OccupancyBuffer.getBuffer(), 1, &region);

            region.srcOffset = buffer.offset + occupancyBufferSize;
            region.size = colourBufferSize;
            vkCmdCopyBuffer(cmd, buffer.buffer, m_ColourBuffer.getBuffer(), 1, &region);
        });
}

void GridAS::freeBuffers()
{
    m_ColourBuffer.cleanup();
    m_OccupancyBuffer.cleanup();
}

void GridAS::createDescriptorSets()
{
    VkDescriptorSetAllocateInfo descriptorSetAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = p_Info.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_BufferSetLayout,
    };

    VK_CHECK(vkAllocateDescriptorSets(p_Info.device, &descriptorSetAI, &m_BufferSet),
        "Failed to allocate descriptor set");

    VkDescriptorBufferInfo occupancyBI {
        .buffer = m_OccupancyBuffer.getBuffer(),
        .offset = 0,
        .range = m_OccupancyBuffer.getSize(),
    };

    VkDescriptorBufferInfo colourBI {
        .buffer = m_ColourBuffer.getBuffer(),
        .offset = 0,
        .range = m_ColourBuffer.getSize(),
    };

    std::vector<VkWriteDescriptorSet> writeSets = {
        {
         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = nullptr,
         .dstSet = m_BufferSet,
         .dstBinding = 0,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = nullptr,
         .pBufferInfo = &occupancyBI,
         .pTexelBufferView = nullptr,
         },
        {
         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = nullptr,
         .dstSet = m_BufferSet,
         .dstBinding = 1,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = nullptr,
         .pBufferInfo = &colourBI,
         .pTexelBufferView = nullptr,
         }
    };

    vkUpdateDescriptorSets(p_Info.device, writeSets.size(), writeSets.data(), 0, nullptr);
    Debug::setDebugName(p_Info.device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)m_BufferSet,
        "Grid buffer descriptor set");
}

void GridAS::freeDescriptorSets()
{
    if (m_BufferSet == VK_NULL_HANDLE)
        return;
    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_BufferSet);
}

void GridAS::createRenderPipelineLayout()
{
    VkPushConstantRange pushConstantRange {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    std::vector<VkDescriptorSetLayout> descriptorsSetLayouts = {
        p_Info.renderDescriptorLayout,
        m_BufferSetLayout,
    };

    VkPipelineLayoutCreateInfo layoutCI {};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.pNext = nullptr;
    layoutCI.flags = 0;
    layoutCI.setLayoutCount = descriptorsSetLayouts.size();
    layoutCI.pSetLayouts = descriptorsSetLayouts.data();
    layoutCI.pushConstantRangeCount = 1;
    layoutCI.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(p_Info.device, &layoutCI, nullptr, &m_RenderPipelineLayout),
        "Failed to create render pipeline layout");

    Debug::setDebugName(p_Info.device, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
        (uint64_t)m_RenderPipelineLayout, "Grid render pipeline layout");
}

void GridAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
}

void GridAS::createRenderPipeline()
{
    VkShaderModule shaderModule = ShaderManager::getInstance()->getShaderModule("grid_AS");

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
                 p_Info.device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_RenderPipeline),
        "Failed to create Grid render pipeline");

    Debug::setDebugName(
        p_Info.device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_RenderPipeline, "Grid render pipeline");
}

void GridAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}
