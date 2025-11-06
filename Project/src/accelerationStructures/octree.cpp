#include "octree.hpp"
#include <memory>
#include <queue>
#include <unistd.h>
#include <variant>
#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>
#include <glm/gtx/matrix_operation.hpp>

#include "../debug_utils.hpp"
#include "../logger.hpp"
#include "../shader_manager.hpp"
#include "accelerationStructure.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/matrix.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::vec3 cameraForward;
    alignas(16) glm::vec3 cameraRight;
    alignas(16) glm::vec3 cameraUp;
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
    freeDescriptorSet();
    freeBuffers();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("octree_AS");
}

void OctreeAS::init(ASStructInfo info)
{
    m_Info = info;

    createDescriptorLayout();

    createRenderPipelineLayout();
    ShaderManager::getInstance()->addModule("octree_AS",
        std::bind(&OctreeAS::createRenderPipeline, this),
        std::bind(&OctreeAS::destroyRenderPipeline, this));
    createRenderPipeline();
}

void OctreeAS::fromLoader(Loader& loader)
{
    std::chrono::steady_clock timer;

    auto start = timer.now();

    struct IntermediaryNode {
        glm::u8vec3 colour;
        bool visible;
        bool parent;
        uint8_t childMask;
        uint32_t childStartIndex = 0;
    };

    freeBuffers();
    freeDescriptorSet();

    uint64_t current_code = 0;
    const glm::uvec3 dimensions = loader.getDimensions();
    uint64_t final_code = dimensions.x * dimensions.y * dimensions.z;

    const uint32_t max_depth = 23;
    uint32_t current_depth = max_depth - 1;

    std::array<std::deque<IntermediaryNode>, max_depth> queues;
    std::vector<IntermediaryNode> intermediaryNodes;

    m_VoxelCount = 0;

    std::function<IntermediaryNode(const std::optional<Voxel>)> convert
        = [](const std::optional<Voxel> v) {
              if (v.has_value()) {
                  glm::vec3 colour = v.value().colour;
                  return IntermediaryNode {
                      .colour = glm::u8vec3 { colour.x * 255, colour.y * 255, colour.z * 255 },
                      .visible = 1,
                      .parent = false,
                      .childMask = 0,
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
              }

              return std::optional<IntermediaryNode>(IntermediaryNode {
                  .colour = colour, .visible = allVisible, .parent = false, .childMask = 0 });
          };

    while (current_code != final_code) {
        const auto current_voxel = loader.getVoxelMorton(current_code);
        current_code++;

        current_depth = max_depth - 1;
        queues[current_depth].push_back(convert(current_voxel));

        if (current_code % 5000000 == 0) {
            auto current = timer.now();

            std::chrono::duration<float, std::milli> difference = current - start;
            float timeElapsed = difference.count() / 1000.0f;
            float percent = (float)current_code / (float)final_code;
            float timeRemaining = (timeElapsed / percent) - timeElapsed;
            LOG_INFO(
                "Percent searched: {:6.6f} | Time elapsed: {:6.3f}s | Time remaining: {:6.3f}s",
                percent, timeElapsed, timeRemaining);
        }

        while (current_depth > 0 && queues[current_depth].size() == 8) {
            IntermediaryNode node;

            bool shouldContinue = true;
            const auto& possible_parent_node = allEqual(queues[current_depth]);

            if (possible_parent_node.has_value()) {
                queues[current_depth - 1].push_back(possible_parent_node.value());
                shouldContinue = false;
            }

            if (shouldContinue) {
                uint8_t childMask = 0;
                for (int8_t i = 0; i < 8; i++) {
                    if (queues[current_depth].at(i).visible) {
                        childMask |= (1 << i);
                        intermediaryNodes.push_back(queues[current_depth].at(i));
                        if (!queues[current_depth].at(i).parent
                            && queues[current_depth].at(i).visible) {
                            m_VoxelCount += pow(8, 22 - current_depth);
                        }
                    }
                }

                IntermediaryNode parent = {
                    .visible = true,
                    .parent = true,
                    .childMask = childMask,
                    .childStartIndex = (uint32_t)(intermediaryNodes.size() - 1),
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
        m_VoxelCount += pow(8, 22 - current_depth);
    }

    m_Nodes.clear();
    m_Nodes.reserve(intermediaryNodes.size());

    // BROKEN: Doesn't work if offset is greater then 2^21
    size_t offset = 0;
    for (ssize_t i = intermediaryNodes.size() - 1; i >= 0; i--) {
        const IntermediaryNode& node = intermediaryNodes[i];

        if (node.parent) {
            size_t target = i - node.childStartIndex + offset;
            assert(target < 0x200000 && "Far pointers not handled");
            m_Nodes.push_back(OctreeNode(node.childMask, target));
        } else if (node.visible) {
            m_Nodes.push_back(OctreeNode(node.colour.x, node.colour.y, node.colour.z));
        }
    }
    auto end = timer.now();

    std::chrono::duration<float, std::milli> difference = end - start;
    LOG_INFO("Octree generation time: {}", difference.count() / 1000.0f);

    createBuffers();
    createDescriptorSet();
}

void OctreeAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
{
    beginCmdDebugLabel(cmd, "Octree AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    glm::vec3 scale = glm::vec3(10);

    glm::mat4 octreeWorld = glm::mat4(1);
    octreeWorld = glm::scale(octreeWorld, scale);
    octreeWorld = glm::translate(octreeWorld, glm::vec3(-1));
    glm::mat4 octreeWorldInverse = glm::inverse(octreeWorld);

    glm::mat4 octreeScaleInverse = glm::inverse(glm::scale(glm::mat4(1), scale));

    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .cameraForward = camera.getForwardVector(),
        .cameraRight = camera.getRightVector(),
        .cameraUp = camera.getUpVector(),
        .octreeWorld = octreeWorld,
        .octreeWorldInverse = octreeWorldInverse,
        .octreeScaleInverse = octreeScaleInverse,
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

uint64_t OctreeAS::getMemoryUsage() { return m_OctreeBuffer.getSize(); }
uint64_t OctreeAS::getVoxels() { return m_VoxelCount; }

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

void OctreeAS::freeBuffers()
{
    m_StagingBuffer.cleanup();
    m_OctreeBuffer.cleanup();
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
    if (m_BufferSet == VK_NULL_HANDLE)
        return;

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
