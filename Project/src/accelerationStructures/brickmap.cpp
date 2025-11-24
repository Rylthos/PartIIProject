#include "brickmap.hpp"

#include <vector>

#include "../compute_pipeline.hpp"
#include "../debug_utils.hpp"
#include "../descriptor_layout.hpp"
#include "../descriptor_set.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"
#include "spdlog/spdlog.h"

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
    freeDescriptorSet();
    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    freeBuffers();

    ShaderManager::getInstance()->removeModule("brickmap_AS");
}

void BrickmapAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();

    ShaderManager::getInstance()->addModule("brickmap_AS",
        std::bind(&BrickmapAS::createRenderPipeline, this),
        std::bind(&BrickmapAS::destroyRenderPipeline, this));

    createRenderPipelineLayout();
    createRenderPipeline();
}

void BrickmapAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    p_FinishedGeneration = false;
    ShaderManager::getInstance()->removeMacro("BRICKMAP_FINISHED_GENERATION");

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              generate(stoken, std::move(loader));
          });
}

void BrickmapAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Brickmap AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    // glm::vec3 scale = glm::vec3(10);
    //
    // glm::mat4 brickgridWorld = glm::mat4(1);
    // brickgridWorld = glm::scale(brickgridWorld, scale);
    // brickgridWorld = glm::translate(brickgridWorld, glm::vec3(-1));
    // glm::mat4 brickgridWorldInverse = glm::inverse(brickgridWorld);
    //
    // glm::mat4 brickgridScaleInverse = glm::inverse(glm::scale(glm::mat4(1), scale));
    //
    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .brickgridWorld = glm::mat4(1.f),
        .brickgridWorldInverse = glm::mat4(1.f),
        .brickgridScaleInverse = glm::mat4(1.f),
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

void BrickmapAS::update(float dt)
{
    if (m_UpdateBuffers) {
        freeBuffers();
        freeDescriptorSet();

        ShaderManager::getInstance()->defineMacro("BRICKMAP_GENERATION_FINISHED");
        updateShaders();

        createBuffers();
        createDescriptorSet();

        p_FinishedGeneration = true;
        m_UpdateBuffers = false;
        p_Generating = false;
    }
}

void BrickmapAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("brickmap_AS"); }

void BrickmapAS::createDescriptorLayout()
{
    m_BufferSetLayout = DescriptorLayoutGenerator::start(p_Info.device)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 0)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 1)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 2)
                            .setDebugName("Brickmap descriptor set layout")
                            .build();
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
                           .setShader("brickmap_AS")
                           .setDebugName("Brickmap render pipeline")
                           .build();
}

void BrickmapAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}

void BrickmapAS::generate(std::stop_token stoken, std::unique_ptr<Loader> loader)
{
    std::chrono::steady_clock timer;
    auto start = timer.now();

    glm::uvec3 dimensions = loader->getDimensionsDiv8();

    m_BrickgridSize = glm::uvec3(glm::ceil(glm::vec3(dimensions) / 8.f));

    size_t totalNodes = m_BrickgridSize.x * m_BrickgridSize.y * m_BrickgridSize.z;

    m_Brickgrid.assign(totalNodes, 0x1);

    size_t index = 0;
    size_t totalColours = 0;
    std::vector<uint8_t> colours;
    colours.resize(8 * 8 * 8 * 3);
    for (uint32_t bY = 0; bY < m_BrickgridSize.y; bY++) {
        for (uint32_t bZ = 0; bZ < m_BrickgridSize.z; bZ++) {
            for (uint32_t bX = 0; bX < m_BrickgridSize.x; bX++) {
                if (stoken.stop_requested())
                    return;

                glm::ivec3 brickWorld = glm::ivec3(bX, bY, bZ) * 8;

                uint32_t usedColours = 0;
                uint64_t occupancy[8];

                uint64_t colourPtr = totalColours;

                {
                    p_GenerationCompletion = (index + 1) / (float)totalNodes;
                    auto current = timer.now();
                    std::chrono::duration<float, std::milli> difference = current - start;
                    p_GenerationTime = difference.count() / 1000.0f;
                }

                for (uint64_t y = 0; y < 8; y++) {
                    occupancy[y] = 0;
                    for (uint64_t z = 0; z < 8; z++) {
                        for (uint64_t x = 0; x < 8; x++) {
                            glm::ivec3 coordinates = brickWorld + glm::ivec3(x, y, z);

                            auto voxel = loader->getVoxel(coordinates);

                            if (voxel.has_value()) {
                                occupancy[y] |= ((uint64_t)1) << ((z * 8) + x);

                                glm::vec3 colour = voxel.value().colour;

                                colours[usedColours * 3 + 0] = std::ceil(colour.r * 255);
                                colours[usedColours * 3 + 1] = std::ceil(colour.g * 255);
                                colours[usedColours * 3 + 2] = std::ceil(colour.b * 255);
                                usedColours++;
                                totalColours++;
                            }
                        }
                    }
                }

                if (usedColours > 0) {
                    Brickmap brick;
                    brick.colourPtr = colourPtr;
                    memcpy(&brick.occupancy, occupancy, sizeof(uint64_t) * 8);
                    brick.colour.resize(usedColours * 3);
                    std::copy(
                        colours.begin(), colours.begin() + (usedColours * 3), brick.colour.begin());

                    m_Brickmaps.push_back(brick);

                    m_Brickgrid[index] = 0x1 | (m_Brickmaps.size() << 1);
                }

                index++;
            }
        }
    }

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;
    p_GenerationTime = difference.count() / 1000.0f;

    m_UpdateBuffers = true;
}
