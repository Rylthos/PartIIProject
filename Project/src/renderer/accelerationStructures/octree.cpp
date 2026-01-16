#include "octree.hpp"

#include <deque>
#include <memory>
#include <stop_token>
#include <unistd.h>
#include <variant>
#include <vulkan/vulkan_core.h>

#include "serializers/octree.hpp"

#include "glm/ext/matrix_transform.hpp"
#include "glm/integer.hpp"
#include "glm/matrix.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/matrix_operation.hpp>

#include "../compute_pipeline.hpp"
#include "../debug_utils.hpp"
#include "../descriptor_layout.hpp"
#include "../descriptor_set.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"
#include "../shader_manager.hpp"
#include "acceleration_structure.hpp"

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::mat4 octreeWorld;
    alignas(16) glm::mat4 octreeWorldInverse;
    alignas(16) glm::mat4 octreeScaleInverse;
    VkDeviceAddress hitDataAddress;
};

OctreeAS::OctreeAS() { }

OctreeAS::~OctreeAS()
{
    p_GenerationThread.request_stop();
    p_FileThread.request_stop();

    freeDescriptorSet();
    freeBuffers();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("AS/octree_AS");
}

void OctreeAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();

    createRenderPipelineLayout();

    ShaderManager::getInstance()->removeMacro("GENERATION_FINISHED");
    ShaderManager::getInstance()->addModule("AS/octree_AS",
        std::bind(&OctreeAS::createRenderPipeline, this),
        std::bind(&OctreeAS::destroyRenderPipeline, this));

    createRenderPipeline();
}

void OctreeAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    {
        std::lock_guard lock(p_Info.graphicsQueue->getLock());
        vkQueueWaitIdle(p_Info.graphicsQueue->getQueue());
    }

    reset();

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              m_Nodes = Generators::generateOctree(
                  stoken, std::move(loader), p_GenerationInfo, m_Dimensions, m_UpdateBuffers);
          });
}

void OctreeAS::fromRaw(const std::vector<uint8_t>& rawData)
{
    {
        std::lock_guard lock(p_Info.graphicsQueue->getLock());
        vkQueueWaitIdle(p_Info.graphicsQueue->getQueue());
    }

    p_RawThread.request_stop();

    reset();

    p_RawThread = std::jthread([this, rawData](std::stop_token stoken) {
        p_Loading = true;
        Serializers::SerialInfo info;
        auto data = Serializers::loadOctree(rawData);

        if (!data.has_value() || stoken.stop_requested()) {
            return;
        }

        std::tie(info, m_Nodes) = data.value();

        m_Dimensions = info.dimensions;

        p_GenerationInfo.voxelCount = info.voxels;
        p_GenerationInfo.nodes = info.nodes;
        p_GenerationInfo.generationTime = 0;
        p_GenerationInfo.completionPercent = 1;

        p_CurrentFrame = 0;

        m_UpdateBuffers = true;
        p_Loading = false;
    });
}

void OctreeAS::fromFile(std::filesystem::path path)
{
    {
        std::lock_guard lock(p_Info.graphicsQueue->getLock());
        vkQueueWaitIdle(p_Info.graphicsQueue->getQueue());
    }

    p_FileThread.request_stop();

    p_FileThread = std::jthread([this, path](std::stop_token stoken) {
        p_Loading = true;

        std::ifstream inputStream = Serializers::loadOctreeFile(path);
        std::vector<uint8_t> data = Serializers::vectorFromStream(inputStream);

        fromRaw(data);
    });
}

void OctreeAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Octree AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    glm::mat4 octreeWorld = glm::mat4(1);
    octreeWorld = glm::scale(octreeWorld, glm::vec3(m_Dimensions));
    octreeWorld = glm::translate(octreeWorld, glm::vec3(-1));
    glm::mat4 octreeWorldInverse = glm::inverse(octreeWorld);

    glm::mat4 octreeScaleInverse = glm::inverse(glm::scale(glm::mat4(1), glm::vec3(m_Dimensions)));

    PushConstants pushConstant = {
        .cameraPosition = camera.getPosition(),
        .octreeWorld = octreeWorld,
        .octreeWorldInverse = octreeWorldInverse,
        .octreeScaleInverse = octreeScaleInverse,
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
}

void OctreeAS::update(float dt)
{
    if (m_UpdateBuffers) {
        {
            std::lock_guard lock(p_Info.graphicsQueue->getLock());
            vkQueueWaitIdle(p_Info.graphicsQueue->getQueue());
        }

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

void OctreeAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("AS/octree_AS"); }

void OctreeAS::createDescriptorLayout()
{
    m_BufferSetLayout = DescriptorLayoutGenerator::start(p_Info.device)
                            .addStorageBufferBinding(VK_SHADER_STAGE_COMPUTE_BIT, 0)
                            .setDebugName("Octree descriptor set layout")
                            .build();
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
    m_BufferSet
        = DescriptorSetGenerator::start(p_Info.device, p_Info.descriptorPool, m_BufferSetLayout)
              .addBufferDescriptor(0, m_OctreeBuffer)
              .setDebugName("Octree descriptor set")
              .build();
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
    m_RenderPipeline = ComputePipelineGenerator::start(p_Info.device, m_RenderPipelineLayout)
                           .setShader("AS/octree_AS")
                           .setDebugName("Octree render pipeline")
                           .build();
}

void OctreeAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}
