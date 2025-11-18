#include "octree.hpp"
#include <algorithm>
#include <memory>
#include <queue>
#include <unistd.h>
#include <variant>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>
#include <glm/gtx/matrix_operation.hpp>

#include "../debug_utils.hpp"
#include "../frame_commands.hpp"
#include "../logger.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"
#include "acceleration_structure.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/integer.hpp"
#include "glm/matrix.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::mat4 octreeWorld;
    alignas(16) glm::mat4 octreeWorldInverse;
    alignas(16) glm::mat4 octreeScaleInverse;
};

OctreeNode::OctreeNode(uint32_t ptr) { m_CurrentType = ptr; }

OctreeNode::OctreeNode(uint8_t childMask, uint32_t offset)
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
        uint32_t flags = (((uint32_t)node->flags & 0x3) << 30);
        assert(node->offset < 0x3FFFFF && "Incorrect pointer value given");
        uint32_t childMask = ((uint32_t)node->childMask & 0xFF) << 22;
        uint32_t offset = ((uint32_t)node->offset & 0x3FFFFF) << 0;
        return flags | childMask | offset;
    } else if (const LeafType* leaf = std::get_if<LeafType>(&m_CurrentType)) {
        uint32_t flags = ((uint32_t)leaf->flags & 0x3) << 30;
        uint32_t r = ((uint32_t)leaf->r) << 16;
        uint32_t g = ((uint32_t)leaf->g) << 8;
        uint32_t b = ((uint32_t)leaf->b) << 0;
        return flags | r | g | b;
    } else if (const uint32_t* ptr = std::get_if<uint32_t>(&m_CurrentType)) {
        return *ptr;
    } else {
        assert(false && "Not possible");
    }
}

OctreeAS::OctreeAS() { }

OctreeAS::~OctreeAS()
{
    p_GenerationThread.request_stop();

    freeDescriptorSet();
    freeBuffers();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("octree_AS");
}

void OctreeAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();

    createRenderPipelineLayout();
    ShaderManager::getInstance()->addModule("octree_AS",
        std::bind(&OctreeAS::createRenderPipeline, this),
        std::bind(&OctreeAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void OctreeAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    p_FinishedGeneration = false;
    ShaderManager::getInstance()->removeMacro("OCTREE_GENERATION_FINISHED");

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              generateNodes(stoken, std::move(loader));
          });
}

void OctreeAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Octree AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    glm::vec3 scale = glm::vec3(10);

    glm::mat4 octreeWorld = glm::mat4(1);
    octreeWorld = glm::scale(octreeWorld, scale);
    octreeWorld = glm::translate(octreeWorld, glm::vec3(-1));
    glm::mat4 octreeWorldInverse = glm::inverse(octreeWorld);

    glm::mat4 octreeScaleInverse = glm::inverse(glm::scale(glm::mat4(1), scale));

    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .octreeWorld = octreeWorld,
        .octreeWorldInverse = octreeWorldInverse,
        .octreeScaleInverse = octreeScaleInverse,
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

void OctreeAS::update(float dt)
{
    if (m_UpdateBuffers) {
        freeBuffers();
        freeDescriptorSet();

        ShaderManager::getInstance()->defineMacro("OCTREE_GENERATION_FINISHED");
        updateShaders();

        createBuffers();
        createDescriptorSet();
        p_FinishedGeneration = true;
        m_UpdateBuffers = false;
        p_Generating = false;
    }
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

    vkCreateDescriptorSetLayout(p_Info.device, &setLayoutCI, nullptr, &m_BufferSetLayout);
}

void OctreeAS::destroyDescriptorLayout()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_BufferSetLayout, nullptr);
}

void OctreeAS::createBuffers()
{
    VkDeviceSize size = sizeof(uint32_t) * m_Nodes.size();
    m_OctreeBuffer.init(p_Info.device, p_Info.allocator, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    m_OctreeBuffer.setDebugName("Octree node buffer");

    auto bufferIndex = FrameCommands::getInstance()->createStaging(size, [=, this](void* ptr) {
        uint32_t* data = (uint32_t*)ptr;
        for (size_t i = 0; i < m_Nodes.size(); i++) {
            data[i] = m_Nodes[i].getData();
        }
    });

    FrameCommands::getInstance()->stagingEval(
        bufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            VkBufferCopy region {
                .srcOffset = buffer.offset,
                .dstOffset = 0,
                .size = size,
            };
            vkCmdCopyBuffer(cmd, buffer.buffer, m_OctreeBuffer.getBuffer(), 1, &region);
        });
}

void OctreeAS::freeBuffers() { m_OctreeBuffer.cleanup(); }

void OctreeAS::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo setAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = p_Info.descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_BufferSetLayout,
    };
    vkAllocateDescriptorSets(p_Info.device, &setAI, &m_BufferSet);

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
        p_Info.device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void OctreeAS::freeDescriptorSet()
{
    if (m_BufferSet == VK_NULL_HANDLE)
        return;

    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_BufferSet);
}

void OctreeAS::createRenderPipelineLayout()
{
    m_RenderPipelineLayout
        = PipelineLayoutGenerator::start(p_Info.device)
              .addDescriptorLayouts({ p_Info.renderDescriptorLayout, m_BufferSetLayout })
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants))
              .setDebugName("Octree render pipeline layout")
              .build();
}

void OctreeAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
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
                 p_Info.device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &m_RenderPipeline),
        "Failed to create octree render pipeline");

    Debug::setDebugName(p_Info.device, VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_RenderPipeline,
        "Octree render pipeline");
}

void OctreeAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}

void OctreeAS::generateNodes(std::stop_token stoken, std::unique_ptr<Loader> loader)
{
    std::chrono::steady_clock timer;

    auto start = timer.now();

    uint64_t current_code = 0;
    const glm::uvec3 dimensions = loader->getDimensions();
    uint64_t final_code = dimensions.x * dimensions.y * dimensions.z;

    const uint32_t max_depth = 23;
    uint32_t current_depth = max_depth - 1;

    std::array<std::deque<IntermediaryNode>, max_depth> queues;
    std::vector<IntermediaryNode> intermediaryNodes;

    p_VoxelCount = 0;

    std::function<IntermediaryNode(const std::optional<Voxel>)> convert
        = [](const std::optional<Voxel> v) {
              if (v.has_value()) {
                  glm::vec3 colour = v.value().colour;
                  return IntermediaryNode {
                      .colour = glm::u8vec3 { colour.x * 255, colour.y * 255, colour.z * 255 },
                      .visible = 1,
                      .parent = false,
                      .childMask = 0,
                      .childCount = 0,
                  };
              } else
                  return IntermediaryNode { .visible = false };
          };

    std::function<std::optional<IntermediaryNode>(const std::deque<IntermediaryNode>&)> allEqual
        = [](const std::deque<IntermediaryNode>& nodes) {
              assert(nodes.size() == 8);
              if (nodes.at(0).parent)
                  return std::optional<IntermediaryNode> {};

              bool allVisible = nodes.at(0).visible;
              glm::u8vec3 colour = nodes.at(0).colour;
              uint32_t count = 0;
              for (uint32_t i = 1; i < 8; i++) {
                  if (nodes.at(i).parent) {
                      return std::optional<IntermediaryNode> {};
                  }

                  if (nodes.at(i).visible != allVisible) {
                      return std::optional<IntermediaryNode> {};
                  }

                  if (nodes.at(i).colour != colour) {
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

    while (current_code != final_code) {
        if (stoken.stop_requested())
            return;

        const auto current_voxel = loader->getVoxelMorton(current_code);
        current_code++;

        current_depth = max_depth - 1;
        queues[current_depth].push_back(convert(current_voxel));

        auto current = timer.now();

        std::chrono::duration<float, std::milli> difference = current - start;
        p_GenerationCompletion = ((float)current_code / (float)final_code) * (3. / 4.f);
        p_GenerationTime = difference.count() / 1000.0f;

        while (current_depth > 0 && queues[current_depth].size() == 8) {
            if (stoken.stop_requested())
                return;

            IntermediaryNode node;

            const auto& possible_parent_node = allEqual(queues[current_depth]);

            if (possible_parent_node.has_value()) {
                queues[current_depth - 1].push_back(possible_parent_node.value());
            } else {
                uint8_t childMask = 0;
                uint32_t childCount = 0;
                for (int8_t i = 0; i < 8; i++) {
                    if (queues[current_depth].at(i).visible) {
                        childMask |= (1 << i);
                        intermediaryNodes.push_back(queues[current_depth].at(i));
                        if (!queues[current_depth].at(i).parent
                            && queues[current_depth].at(i).visible) {
                            p_VoxelCount += pow(8, 22 - current_depth);
                        }
                        childCount += queues[current_depth].at(i).childCount + 1;
                    }
                }

                IntermediaryNode parent = {
                    .visible = childMask != 0,
                    .parent = true,
                    .childMask = childMask,
                    .childStartIndex = (uint32_t)(intermediaryNodes.size() - 1),
                    .childCount = childCount,
                };

                queues[current_depth - 1].push_back(parent);
            }

            queues[current_depth].clear();
            current_depth--;
        }
    }
    assert(queues[current_depth].size() == 1);
    intermediaryNodes.push_back(queues[current_depth].at(0));
    if (!queues[current_depth].at(0).parent && queues[current_depth].at(0).visible) {
        p_VoxelCount += pow(8, 22 - current_depth);
    }

    m_Nodes.clear();
    m_Nodes.reserve(intermediaryNodes.size());

    {
        const IntermediaryNode& finalNode = intermediaryNodes[intermediaryNodes.size() - 1];
        m_Nodes.push_back(OctreeNode(finalNode.childMask, 1));
    }

    writeChildrenNodes(stoken, intermediaryNodes, intermediaryNodes.size() - 1, timer, start);

    auto end = timer.now();

    std::chrono::duration<float, std::milli> difference = end - start;

    p_GenerationTime = difference.count() / 1000.0f;
    p_GenerationCompletion = 1.f;

    m_UpdateBuffers = true;
}

void OctreeAS::writeChildrenNodes(std::stop_token stoken,
    const std::vector<IntermediaryNode>& nodes, size_t index, std::chrono::steady_clock clock,
    const std::chrono::steady_clock::time_point startTime)
{
    if (stoken.stop_requested())
        return;

    size_t startingIndex = m_Nodes.size();

    const IntermediaryNode& parentNode = nodes.at(index);
    uint8_t childrenCount = glm::bitCount(parentNode.childMask);

    for (uint8_t i = 0; i < childrenCount; i++) {
        size_t childIndex = parentNode.childStartIndex - i;
        const IntermediaryNode childNode = nodes.at(childIndex);

        m_Nodes.push_back(OctreeNode(childNode.colour.r, childNode.colour.g, childNode.colour.b));
    }

    size_t currentOffset = 0;
    uint8_t farPointerCount = 0;
    for (uint8_t i = 0; i < childrenCount; i++) {
        size_t childIndex = parentNode.childStartIndex - i;
        const IntermediaryNode childNode = nodes.at(childIndex);

        // Exceeds normal pointer with room for other nodes to add far pointers
        if (currentOffset >= 0x1F0000) {
            farPointerCount += 1;
            m_Nodes.push_back(OctreeNode(0));
        }

        currentOffset += childNode.childCount + 1;
    }

    auto current = clock.now();
    std::chrono::duration<float, std::milli> difference = current - startTime;
    p_GenerationTime = difference.count() / 1000.0f;
    p_GenerationCompletion = 0.75 + ((nodes.size() - index) / (float)nodes.size()) * 0.25f;

    uint8_t currentFarPointer = 0;
    for (uint8_t i = 0; i < childrenCount; i++) {
        size_t childIndex = parentNode.childStartIndex - i;
        const IntermediaryNode childNode = nodes.at(childIndex);

        if (childNode.parent) {
            size_t childStartingIndex = m_Nodes.size();
            size_t offset = childStartingIndex - (startingIndex + i);
            writeChildrenNodes(stoken, nodes, childIndex, clock, startTime);

            if (offset >= 0x200000) {
                size_t farPointerIndex = startingIndex + childrenCount + currentFarPointer;
                assert(childStartingIndex - farPointerIndex <= 0xFFFFFFFF);

                m_Nodes[farPointerIndex] = OctreeNode(childStartingIndex - farPointerIndex);

                offset = 0x200000 + farPointerIndex - (startingIndex + i);
                currentFarPointer++;
            }

            m_Nodes[startingIndex + i] = OctreeNode(childNode.childMask, offset);
        }
    }
    assert(farPointerCount >= currentFarPointer && "Pointers should match");
}
