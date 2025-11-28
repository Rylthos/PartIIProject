#include "contree.hpp"

#include <deque>
#include <memory>

#include <vulkan/vulkan_core.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/matrix_operation.hpp>

#include "../compute_pipeline.hpp"
#include "../debug_utils.hpp"
#include "../descriptor_layout.hpp"
#include "../descriptor_set.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
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
    p_GenerationThread.request_stop();

    freeDescriptorSet();
    destroyBuffers();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("contree_AS");
}

void ContreeAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();
    createRenderPipelineLayout();

    ShaderManager::getInstance()->removeMacro("CONTREE_GENERATION_FINISHED");
    ShaderManager::getInstance()->addModule("contree_AS",
        std::bind(&ContreeAS::createRenderPipeline, this),
        std::bind(&ContreeAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void ContreeAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    p_FinishedGeneration = false;

    ShaderManager::getInstance()->removeMacro("CONTREE_GENERATION_FINISHED");
    updateShaders();

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              generateNodes(stoken, std::move(loader));
          });
}

void ContreeAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Contree AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);

    std::vector<VkDescriptorSet> descriptorSets = {
        renderSet,
    };
    if (p_FinishedGeneration) {
        descriptorSets.push_back(m_BufferSet);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    glm::mat4 contreeWorld = glm::mat4(1);
    contreeWorld = glm::scale(contreeWorld, glm::vec3(m_Dimensions));
    contreeWorld = glm::translate(contreeWorld, glm::vec3(-1));
    glm::mat4 contreeWorldInverse = glm::inverse(contreeWorld);

    glm::mat4 contreeScaleInverse = glm::inverse(glm::scale(glm::mat4(1), glm::vec3(m_Dimensions)));

    PushConstants pushConstants = {
        .cameraPosition = camera.getPosition(),
        .contreeWorld = contreeWorld,
        .contreeWorldInverse = contreeWorldInverse,
        .contreeScaleInverse = contreeScaleInverse,
    };
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstants);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    Debug::endCmdDebugLabel(cmd);
}

void ContreeAS::update(float dt)
{
    if (m_UpdateBuffers) {
        freeDescriptorSet();
        destroyBuffers();

        ShaderManager::getInstance()->defineMacro("CONTREE_GENERATION_FINISHED");
        updateShaders();

        createBuffers();
        createDescriptorSet();
        p_FinishedGeneration = true;
        m_UpdateBuffers = false;
        p_Generating = false;
    }
}

void ContreeAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("contree_AS"); }

void ContreeAS::createDescriptorLayout()
{
    m_BufferSetLayout = DescriptorLayoutGenerator::start(p_Info.device)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 0)
                            .setDebugName("Contree buffer set layout")
                            .build();
}

void ContreeAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void ContreeAS::createBuffers()
{
    VkDeviceSize size = sizeof(uint64_t) * 2 * m_Nodes.size();
    m_ContreeBuffer.init(p_Info.device, p_Info.allocator, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_ContreeBuffer.setDebugName("Contree node buffer");

    auto bufferIndex = FrameCommands::getInstance()->createStaging(size, [=, this](void* ptr) {
        uint64_t* data = (uint64_t*)ptr;
        for (size_t i = 0; i < m_Nodes.size(); i++) {
            const auto& node = m_Nodes[i].getData();
            data[i * 2] = node[0];
            data[i * 2 + 1] = node[1];
        }
    });

    FrameCommands::getInstance()->stagingEval(
        bufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = size,
            };
            vkCmdCopyBuffer(cmd, buffer.buffer, m_ContreeBuffer.getBuffer(), 1, &region);
        });
}

void ContreeAS::destroyBuffers() { m_ContreeBuffer.cleanup(); }

void ContreeAS::createDescriptorSet()
{
    m_BufferSet
        = DescriptorSetGenerator::start(p_Info.device, p_Info.descriptorPool, m_BufferSetLayout)
              .addBufferDescriptor(0, m_ContreeBuffer)
              .setDebugName("Contree buffer set")
              .build();
}

void ContreeAS::freeDescriptorSet()
{
    if (m_BufferSet == VK_NULL_HANDLE)
        return;

    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_BufferSet);
}

void ContreeAS::createRenderPipelineLayout()
{
    m_RenderPipelineLayout
        = PipelineLayoutGenerator::start(p_Info.device)
              .addDescriptorLayouts({ p_Info.renderDescriptorLayout, m_BufferSetLayout })
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants))
              .setDebugName("Contree render pipeline layout")
              .build();
}

void ContreeAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
}

void ContreeAS::createRenderPipeline()
{
    m_RenderPipeline = ComputePipelineGenerator::start(p_Info.device, m_RenderPipelineLayout)
                           .setShader("contree_AS")
                           .setDebugName("Contree render pipeline")
                           .build();
}

void ContreeAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}

void ContreeAS::generateNodes(std::stop_token stoken, std::unique_ptr<Loader> loader)
{
    std::chrono::steady_clock timer;

    m_Dimensions = loader->getDimensionsDiv4();

    auto start = timer.now();

    uint64_t currentCode = 0;

    uint64_t finalCode = m_Dimensions.x * m_Dimensions.y * m_Dimensions.z;

    const uint32_t maxDepth = 11;
    uint32_t currentDepth = maxDepth;

    std::array<std::deque<IntermediaryNode>, maxDepth> queues;
    std::vector<IntermediaryNode> intermediaryNodes;

    p_VoxelCount = 0;

    std::function<IntermediaryNode(const std::optional<Voxel>)> convert
        = [](const std::optional<Voxel> v) {
              if (v.has_value()) {
                  glm::vec3 colour = v.value().colour;
                  return IntermediaryNode {
                      .colour = colour,
                      .visible = true,
                      .parent = false,
                      .childMask = 0,
                  };
              } else {
                  return IntermediaryNode {
                      .visible = false,
                  };
              }
          };

    std::function<std::optional<IntermediaryNode>(const std::deque<IntermediaryNode>&)> allEqual
        = [](const std::deque<IntermediaryNode>& nodes) {
              assert(nodes.size() == 64);
              bool allVisible = nodes.at(0).visible;
              glm::vec3 colour = nodes.at(0).colour;
              uint32_t count = 0;

              for (uint32_t i = 0; i < 64; i++) {
                  if (nodes.at(i).parent || nodes.at(i).visible != allVisible
                      || nodes.at(i).colour != colour) {
                      return std::optional<IntermediaryNode> {};
                  }
                  count += nodes.at(i).childCount + 1;
              }

              return std::optional<IntermediaryNode>(IntermediaryNode {
                  .colour = colour,
                  .visible = allVisible,
                  .parent = false,
                  .childMask = 0,
                  .childCount = count,
              });
          };

    while (currentCode != finalCode) {
        if (stoken.stop_requested())
            return;

        const auto currentVoxel = loader->getVoxelMorton2(currentCode);
        currentCode++;

        currentDepth = maxDepth - 1;
        queues[currentDepth].push_back(convert(currentVoxel));

        auto current = timer.now();
        std::chrono::duration<float, std::milli> difference = current - start;
        p_GenerationCompletion = ((float)currentCode / (float)finalCode);
        p_GenerationTime = difference.count() / 1000.0f;

        while (currentDepth > 0 && queues[currentDepth].size() == 64) {
            if (stoken.stop_requested())
                return;

            IntermediaryNode node;

            const auto& possibleParentNode = allEqual(queues[currentDepth]);

            if (possibleParentNode.has_value()) {
                queues[currentDepth - 1].push_back(possibleParentNode.value());
            } else {
                uint64_t childMask = 0;
                uint32_t childCount = 0;
                for (int8_t i = 63; i >= 0; i--) {
                    if (queues[currentDepth].at(i).visible) {
                        childMask |= (1ull << i);
                        intermediaryNodes.push_back(queues[currentDepth].at(i));
                        if (!queues[currentDepth].at(i).parent) {
                            p_VoxelCount += pow(64, 10 - currentDepth);
                        }
                        childCount += queues[currentDepth].at(i).childCount + 1;
                    }
                }

                IntermediaryNode parent = {
                    .visible = childMask != 0,
                    .parent = true,
                    .childMask = childMask,
                    .childStartIndex = (uint32_t)(intermediaryNodes.size() - 1),
                    .childCount = childCount,
                };

                queues[currentDepth - 1].push_back(parent);
            }
            queues[currentDepth].clear();
            currentDepth--;
        }
    }
    assert(queues[currentDepth].size() == 1);
    intermediaryNodes.push_back(queues[currentDepth].at(0));
    if (!queues[currentDepth].at(0).parent && queues[currentDepth].at(0).visible) {
        p_VoxelCount += pow(64, 10 - currentDepth);
    }

    m_Nodes.clear();
    m_Nodes.reserve(intermediaryNodes.size());

    size_t index = intermediaryNodes.size() - 1;
    for (auto it = intermediaryNodes.rbegin(); it != intermediaryNodes.rend(); it++) {
        if (stoken.stop_requested())
            return;

        if (it->parent) {
            glm::vec3 c = it->colour * 255.f;
            glm::u8vec3 colour = glm::u8vec3(c.r, c.g, c.b);
            uint32_t targetOffset = index - it->childStartIndex;
            m_Nodes.push_back(
                ContreeNode(it->childMask, targetOffset, colour.r, colour.g, colour.b));
        } else if (it->visible) {
            m_Nodes.push_back(ContreeNode(it->colour.r, it->colour.g, it->colour.b));
        }
        index--;
    }

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;
    p_GenerationTime = difference.count() / 1000.0f;

    m_UpdateBuffers = true;
}
