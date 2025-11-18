#include "brickmap.hpp"

#include "../debug_utils.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"
#include "acceleration_structure.hpp"

#include <vector>

#include <vulkan/vulkan_core.h>

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::mat4 brickgridWorld;
    alignas(16) glm::mat4 brickgridWorldInverse;
    alignas(16) glm::mat4 brickgridScaleInverse;
    alignas(16) glm::uvec3 brickgridSize;
};

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
    Debug::beginCmdDebugLabel(cmd, "Brickmap AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    glm::vec3 scale = glm::vec3(10);

    glm::mat4 brickgridWorld = glm::mat4(1);
    brickgridWorld = glm::scale(brickgridWorld, scale);
    brickgridWorld = glm::translate(brickgridWorld, glm::vec3(-1));
    glm::mat4 brickgridWorldInverse = glm::inverse(brickgridWorld);

    glm::mat4 brickgridScaleInverse = glm::inverse(glm::scale(glm::mat4(1), scale));

    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .brickgridWorld = brickgridWorld,
        .brickgridWorldInverse = brickgridWorldInverse,
        .brickgridScaleInverse = brickgridScaleInverse,
        .brickgridSize = m_BrickgridSize,
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        renderSet,
    };
    if (p_FinishedGeneration) {
        descriptorSets.push_back(m_BufferSet);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstant);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    Debug::endCmdDebugLabel(cmd);
}

void BrickmapAS::update(float dt) { }

void BrickmapAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("brickmap_AS"); }

void BrickmapAS::createDescriptorLayout()
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

    VK_CHECK(vkCreateDescriptorSetLayout(p_Info.device, &setLayoutCI, nullptr, &m_BufferSetLayout),
        "Failed to create buffer set layout");
}

void BrickmapAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void BrickmapAS::createBuffers()
{
    VkDeviceSize gridSize
        = m_BrickgridSize.x * m_BrickgridSize.y * m_BrickgridSize.z * sizeof(BrickgridPtr);
    m_BrickgridBuffer.init(p_Info.device, p_Info.allocator, gridSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    VkDeviceSize brickmapSize = m_Brickmaps.size() * (sizeof(uint64_t) * 8 + sizeof(uint32_t));
    m_BrickmapsBuffer.init(p_Info.device, p_Info.allocator, brickmapSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    size_t colourCount = 0;
    for (const auto& brickmap : m_Brickmaps) {
        colourCount += brickmap.colour.size();
    }

    VkDeviceSize colourSize = colourCount * sizeof(uint8_t) * 3;
    m_ColourBuffer.init(p_Info.device, p_Info.allocator, colourSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    auto gridBufferIndex
        = FrameCommands::getInstance()->createStaging(gridSize, [=, this](void* ptr) {
              BrickgridPtr* data = (BrickgridPtr*)ptr;
              for (size_t i = 0; i < m_Brickgrid.size(); i++) {
                  data[i] = m_Brickgrid[i];
              }
          });

    FrameCommands::getInstance()->stagingEval(
        gridBufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = gridSize,
            };
            vkCmdCopyBuffer(cmd, buffer.buffer, m_BrickgridBuffer.getBuffer(), 1, &region);
        });

    auto mapBufferIndex
        = FrameCommands::getInstance()->createStaging(brickmapSize, [=, this](void* ptr) {
              uint32_t* data = (uint32_t*)ptr;
              const uint32_t nodeSize = 2 * 8 + 1; // Number of uint32_t
              for (size_t i = 0; i < m_Brickmaps.size(); i++) {
                  *(data + i * nodeSize) = m_Brickmaps[i].colourPtr;
                  memcpy(data + i * nodeSize + 1, m_Brickmaps[i].occupancy, sizeof(uint64_t) * 8);
              }
          });

    FrameCommands::getInstance()->stagingEval(
        mapBufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = brickmapSize,
            };
            vkCmdCopyBuffer(cmd, buffer.buffer, m_BrickmapsBuffer.getBuffer(), 1, &region);
        });

    auto colourBufferIndex
        = FrameCommands::getInstance()->createStaging(colourSize, [=, this](void* ptr) {
              uint8_t* data = (uint8_t*)ptr;
              size_t offset = 0;
              for (size_t i = 0; i < m_Brickmaps.size(); i++) {
                  const auto& brickmap = m_Brickmaps[i];
                  for (size_t j = 0; j < brickmap.colour.size(); j++) {
                      const uint8_t* colour = brickmap.colour.at(j);
                      memcpy(data + offset + j * 3, colour, sizeof(uint8_t) * 3);
                  }
                  offset += brickmap.colour.size() * 3;
              }
          });

    FrameCommands::getInstance()->stagingEval(
        colourBufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = colourSize,
            };
            vkCmdCopyBuffer(cmd, buffer.buffer, m_ColourBuffer.getBuffer(), 1, &region);
        });
}

void BrickmapAS::freeBuffers()
{
    m_ColourBuffer.cleanup();
    m_BrickmapsBuffer.cleanup();
    m_BrickgridBuffer.cleanup();
}

void BrickmapAS::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo setAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = p_Info.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_BufferSetLayout,
    };
    vkAllocateDescriptorSets(p_Info.device, &setAI, &m_BufferSet);

    VkDescriptorBufferInfo gridBI {
        .buffer = m_BrickgridBuffer.getBuffer(),
        .offset = 0,
        .range = m_BrickgridBuffer.getSize(),
    };

    VkDescriptorBufferInfo mapBI {
        .buffer = m_BrickmapsBuffer.getBuffer(),
        .offset = 0,
        .range = m_BrickmapsBuffer.getSize(),
    };

    VkDescriptorBufferInfo colourBI {
        .buffer = m_ColourBuffer.getBuffer(),
        .offset = 0,
        .range = m_ColourBuffer.getSize(),
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
         .pBufferInfo = &gridBI,
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
         .pBufferInfo = &mapBI,
         .pTexelBufferView = nullptr,
         },
        {
         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
         .pNext = nullptr,
         .dstSet = m_BufferSet,
         .dstBinding = 2,
         .dstArrayElement = 0,
         .descriptorCount = 1,
         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
         .pImageInfo = nullptr,
         .pBufferInfo = &colourBI,
         .pTexelBufferView = nullptr,
         }
    };

    vkUpdateDescriptorSets(
        p_Info.device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void BrickmapAS::freeDescriptorSet()
{
    if (m_BufferSet == VK_NULL_HANDLE)
        return;

    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_BufferSet);
}

void BrickmapAS::createRenderPipelineLayout()
{
    m_RenderPipelineLayout
        = PipelineLayoutGenerator::start(p_Info.device)
              .addDescriptorLayouts({ p_Info.renderDescriptorLayout, m_BufferSetLayout })
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants))
              .setDebugName("Brickmap render pipeline layout")
              .build();
}

void BrickmapAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
}

void BrickmapAS::createRenderPipeline()
{
    VkShaderModule shaderModule = ShaderManager::getInstance()->getShaderModule("brickmap_AS");

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
        "Failed to create brickmap render pipeline");

    Debug::setDebugName(p_Info.device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_RenderPipeline,
        "Brickmap render pipeline");
}

void BrickmapAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}
