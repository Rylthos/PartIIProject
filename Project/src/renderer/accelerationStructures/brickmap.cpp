#include "brickmap.hpp"

#include <vector>
#include <vulkan/vulkan_core.h>

#include "../compute_pipeline.hpp"
#include "../debug_utils.hpp"
#include "../descriptor_layout.hpp"
#include "../descriptor_set.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"

#include "acceleration_structure.hpp"
#include "serializers/brickmap.hpp"

#include "spdlog/spdlog.h"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::uvec3 brickgridSize;
    VkDeviceAddress hitDataAddress;
};

BrickmapAS::BrickmapAS() { }
BrickmapAS::~BrickmapAS()
{
    freeDescriptorSet();
    destroyDescriptorLayout();

    destroyRequestPipeline();
    destroyRequestPipelineLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    freeBuffers();

    ShaderManager::getInstance()->removeModule("AS/brickmap_AS");
    ShaderManager::getInstance()->removeModule("AS/brickmap_AS_req");
}

void BrickmapAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();

    ShaderManager::getInstance()->removeMacro("GENERATION_FINISHED");

    createRenderPipelineLayout();
    ShaderManager::getInstance()->addModule("AS/brickmap_AS",
        std::bind(&BrickmapAS::createRenderPipeline, this),
        std::bind(&BrickmapAS::destroyRenderPipeline, this));
    createRenderPipeline();

    createRequestPipelineLayout();
    ShaderManager::getInstance()->addModule("AS/brickmap_AS_req",
        std::bind(&BrickmapAS::createRequestPipeline, this),
        std::bind(&BrickmapAS::destroyRequestPipeline, this));
    createRequestPipeline();
}

void BrickmapAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    p_FinishedGeneration = false;

    ShaderManager::getInstance()->removeMacro("GENERATION_FINISHED");
    updateShaders();

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              std::tie(m_Brickgrid, m_Brickmaps) = Generators::generateBrickmap(
                  stoken, std::move(loader), p_GenerationInfo, m_BrickgridSize, m_UpdateBuffers);
          });
}

void BrickmapAS::fromFile(std::filesystem::path path)
{
    p_FileThread.request_stop();
    p_FileThread = std::jthread([this, path](std::stop_token stoken) {
        p_Loading = true;
        Serializers::SerialInfo info;
        auto data = Serializers::loadBrickmap(path);

        if (!data.has_value() || stoken.stop_requested()) {
            return;
        }

        std::tie(info, m_Brickgrid, m_Brickmaps) = data.value();

        m_BrickgridSize = info.dimensions;

        p_GenerationInfo.voxelCount = info.voxels;
        p_GenerationInfo.nodes = info.nodes;
        p_GenerationInfo.generationTime = 0;
        p_GenerationInfo.completionPercent = 1;

        m_UpdateBuffers = true;
        p_Loading = false;
    });
}

void BrickmapAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Brickmap AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .brickgridSize = m_BrickgridSize,
        .hitDataAddress = p_Info.hitDataAddress,
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

    if (p_FinishedGeneration) {
        VkBufferMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
            .srcQueueFamilyIndex = p_Info.graphicsQueueIndex,
            .dstQueueFamilyIndex = p_Info.graphicsQueueIndex,
            .buffer = m_RequestBuffer.getBuffer(),
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };

        std::vector<VkBufferMemoryBarrier> barriers = {
            barrier,
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, barriers.size(), barriers.data(),
            0, nullptr);

        Debug::beginCmdDebugLabel(cmd, "Brickmap Requests", { 0.0f, 0.0f, 1.0f, 1.0f });

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RequestPipeline);
        std::vector<VkDescriptorSet> descriptorSets = {
            m_BufferSet,
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RequestPipelineLayout, 0,
            descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        uint32_t requests = std::ceil(m_Requests / 32.f);
        vkCmdDispatch(cmd, requests, 1, 1);
        m_MappedRequestBuffer[0] = 0;

        Debug::endCmdDebugLabel(cmd);
    }
}

void BrickmapAS::update(float dt)
{
    if (m_UpdateBuffers) {
        freeBuffers();
        freeDescriptorSet();

        ShaderManager::getInstance()->defineMacro("GENERATION_FINISHED");
        updateShaders();

        createBuffers();
        createDescriptorSet();

        p_FinishedGeneration = true;
        m_UpdateBuffers = false;
        p_Generating = false;
    }
}

void BrickmapAS::updateShaders()
{
    ShaderManager::getInstance()->moduleUpdated("AS/brickmap_AS");
    ShaderManager::getInstance()->moduleUpdated("AS/brickmap_AS_req");
}

void BrickmapAS::createDescriptorLayout()
{
    m_BufferSetLayout = DescriptorLayoutGenerator::start(p_Info.device)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 0)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 2)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 3)
                            .setDebugName("Brickmap descriptor set layout")
                            .build();
}

void BrickmapAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void BrickmapAS::createBuffers()
{
    VkDeviceSize gridSize = m_BrickgridSize.x * m_BrickgridSize.y * m_BrickgridSize.z
        * sizeof(Generators::BrickgridPtr);
    m_BrickgridBuffer.init(p_Info.device, p_Info.allocator, gridSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_BrickgridBuffer.setDebugName("Brickgrid Buffer");

    VkDeviceSize brickmapSize = m_Brickmaps.size() * (sizeof(uint64_t) * 9);
    m_BrickmapsBuffer.init(p_Info.device, p_Info.allocator, brickmapSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_BrickmapsBuffer.setDebugName("Brickmap Buffer");

    size_t colourCount = 0;
    for (const auto& brickmap : m_Brickmaps) {
        colourCount += brickmap.colour.size();
    }

    VkDeviceSize colourSize = colourCount * sizeof(uint8_t) * 3;
    m_ColourBuffer.init(p_Info.device, p_Info.allocator, colourSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ColourBuffer.setDebugName("Colour Buffer");

    VkDeviceSize requestSize = 1 + m_Requests;
    m_RequestBuffer.init(p_Info.device, p_Info.allocator, requestSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        VMA_MEMORY_USAGE_AUTO);
    m_MappedRequestBuffer = (uint32_t*)m_RequestBuffer.mapMemory();

    auto gridBufferIndex
        = FrameCommands::getInstance()->createStaging(gridSize, [=, this](void* ptr) {
              Generators::BrickgridPtr* data = (Generators::BrickgridPtr*)ptr;
              for (size_t i = 0; i < m_Brickgrid.size(); i++) {
                  data[i] = m_Brickgrid[i] & 0xFFFFFFFE;
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
              uint64_t* data = (uint64_t*)ptr;
              const uint64_t nodeSize = 9; // Number of uint32_t
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
                      const uint8_t c = brickmap.colour.at(j);
                      data[offset + j] = c;
                  }
                  offset += brickmap.colour.size();
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
    if (m_MappedRequestBuffer) {
        m_MappedRequestBuffer = nullptr;
        m_RequestBuffer.unmapMemory();
        m_RequestBuffer.cleanup();
    }

    m_ColourBuffer.cleanup();
    m_BrickmapsBuffer.cleanup();
    m_BrickgridBuffer.cleanup();
}

void BrickmapAS::createDescriptorSet()
{
    m_BufferSet
        = DescriptorSetGenerator::start(p_Info.device, p_Info.descriptorPool, m_BufferSetLayout)
              .addBufferDescriptor(0, m_BrickgridBuffer)
              .addBufferDescriptor(1, m_BrickmapsBuffer)
              .addBufferDescriptor(2, m_ColourBuffer)
              .addBufferDescriptor(3, m_RequestBuffer)
              .setDebugName("Brickmap descriptor set")
              .build();
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
    m_RenderPipeline = ComputePipelineGenerator::start(p_Info.device, m_RenderPipelineLayout)
                           .setShader("AS/brickmap_AS")
                           .setDebugName("Brickmap render pipeline")
                           .build();
}

void BrickmapAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}

void BrickmapAS::createRequestPipelineLayout()
{
    m_RequestPipelineLayout = PipelineLayoutGenerator::start(p_Info.device)
                                  .addDescriptorLayouts({ m_BufferSetLayout })
                                  .setDebugName("Brickmap request pipeline layout")
                                  .build();
}

void BrickmapAS::destroyRequestPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RequestPipelineLayout, nullptr);
}

void BrickmapAS::createRequestPipeline()
{
    m_RequestPipeline = ComputePipelineGenerator::start(p_Info.device, m_RequestPipelineLayout)
                            .setShader("AS/brickmap_AS_req")
                            .setDebugName("Brickmap request pipeline")
                            .build();
}

void BrickmapAS::destroyRequestPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RequestPipeline, nullptr);
}
