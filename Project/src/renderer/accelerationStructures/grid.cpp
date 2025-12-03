#include "grid.hpp"

#include <vector>

#include "logger/logger.hpp"

#include "serializers/common.hpp"
#include "serializers/grid.hpp"

#include "../compute_pipeline.hpp"
#include "../debug_utils.hpp"
#include "../descriptor_layout.hpp"
#include "../descriptor_set.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"

#include <vulkan/vulkan_core.h>

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

    ShaderManager::getInstance()->removeMacro("GRID_GENERATION_FINISHED");
    ShaderManager::getInstance()->addModule("grid_AS",
        std::bind(&GridAS::createRenderPipeline, this),
        std::bind(&GridAS::destroyRenderPipeline, this));

    createRenderPipeline();
}

void GridAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    p_FinishedGeneration = false;

    ShaderManager::getInstance()->removeMacro("GRID_GENERATION_FINISHED");
    updateShaders();

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              m_Voxels = Generators::generateGrid(
                  stoken, std::move(loader), p_GenerationInfo, m_Dimensions, m_UpdateBuffers);
          });
}

void GridAS::fromFile(std::filesystem::path path)
{
    Serializers::SerialInfo info;
    auto data = Serializers::loadGrid(path);

    if (!data.has_value()) {
        return;
    }

    std::tie(info, m_Voxels) = data.value();

    m_Dimensions = info.dimensions;

    p_GenerationInfo.voxelCount = info.voxels;
    p_GenerationInfo.nodes = info.nodes;
    p_GenerationInfo.generationTime = 0;
    p_GenerationInfo.completionPercent = 1;

    m_UpdateBuffers = true;
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

void GridAS::update(float dt)
{
    if (m_UpdateBuffers) {
        freeBuffers();
        freeDescriptorSets();

        ShaderManager::getInstance()->defineMacro("GRID_GENERATION_FINISHED");
        updateShaders();

        createBuffer();
        createDescriptorSets();
        p_FinishedGeneration = true;
        m_UpdateBuffers = false;
        p_Generating = false;
    }
}

void GridAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("grid_AS"); }

void GridAS::createDescriptorLayouts()
{
    m_BufferSetLayout = DescriptorLayoutGenerator::start(p_Info.device)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 0)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                            .setDebugName("Grid descriptor set layout")
                            .build();
}

void GridAS::destroyDescriptorLayouts()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void GridAS::createBuffer()
{
    VkDeviceSize occupancyBufferSize
        = std::ceil(m_Voxels.size() / 32.) * sizeof(uint32_t); // Convert to bytes
    VkDeviceSize colourBufferSize = sizeof(glm::vec3) * m_Voxels.size();

    m_OccupancyBuffer.init(p_Info.device, p_Info.allocator, occupancyBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_OccupancyBuffer.setDebugName("Grid occupancy buffer");

    m_ColourBuffer.init(p_Info.device, p_Info.allocator, colourBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ColourBuffer.setDebugName("Grid colour buffer");

    auto occupancyIndex
        = FrameCommands::getInstance()->createStaging(occupancyBufferSize, [=, this](void* ptr) {
              uint32_t* dataOccupancy = (uint32_t*)ptr;

              uint32_t current_index = 0;
              uint32_t current_mask = 0;
              for (size_t i = 0; i < m_Voxels.size(); i++) {
                  // dataColour[i * 3 + 0] = m_Voxels[i].colour.x;
                  // dataColour[i * 3 + 1] = m_Voxels[i].colour.y;
                  // dataColour[i * 3 + 2] = m_Voxels[i].colour.z;
                  //
                  if ((i / 32) != current_index) {
                      dataOccupancy[current_index] = current_mask;
                      current_index = i / 32;
                      current_mask = 0;
                  }

                  current_mask |= (m_Voxels[i].visible & 1) << (i % 32);
              }
              dataOccupancy[current_index] = current_mask;
          });

    FrameCommands::getInstance()->stagingEval(
        occupancyIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = occupancyBufferSize,
            };

            vkCmdCopyBuffer(cmd, buffer.buffer, m_OccupancyBuffer.getBuffer(), 1, &region);
        });

    auto colourIndex
        = FrameCommands::getInstance()->createStaging(colourBufferSize, [=, this](void* ptr) {
              float* data = (float*)ptr;
              for (size_t i = 0; i < m_Voxels.size(); i++) {
                  data[i * 3 + 0] = m_Voxels.at(i).colour.r;
                  data[i * 3 + 1] = m_Voxels.at(i).colour.g;
                  data[i * 3 + 2] = m_Voxels.at(i).colour.b;
              }
          });

    FrameCommands::getInstance()->stagingEval(
        colourIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = colourBufferSize,
            };

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
    m_BufferSet
        = DescriptorSetGenerator::start(p_Info.device, p_Info.descriptorPool, m_BufferSetLayout)
              .addBufferDescriptor(0, m_OccupancyBuffer)
              .addBufferDescriptor(1, m_ColourBuffer)
              .setDebugName("Grid descriptor set")
              .build();
}

void GridAS::freeDescriptorSets()
{
    if (m_BufferSet == VK_NULL_HANDLE)
        return;
    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_BufferSet);
}

void GridAS::createRenderPipelineLayout()
{
    m_RenderPipelineLayout
        = PipelineLayoutGenerator::start(p_Info.device)
              .addDescriptorLayouts({ p_Info.renderDescriptorLayout, m_BufferSetLayout })
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants))
              .setDebugName("Grid render pipeline layout")
              .build();
}

void GridAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
}

void GridAS::createRenderPipeline()
{
    m_RenderPipeline = ComputePipelineGenerator::start(p_Info.device, m_RenderPipelineLayout)
                           .setShader("grid_AS")
                           .setDebugName("Grid render pipeline")
                           .build();
}

void GridAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}
