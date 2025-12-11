#include "contree.hpp"

#include <deque>
#include <memory>

#include <vulkan/vulkan_core.h>

#include "serializers/contree.hpp"

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
    VkDeviceAddress hitDataAddress;
};

ContreeAS::ContreeAS() { }
ContreeAS::~ContreeAS()
{
    p_GenerationThread.request_stop();

    freeDescriptorSet();
    destroyBuffers();

    destroyDescriptorLayout();

    destroyRenderPipeline();
    destroyRenderPipelineLayout();

    ShaderManager::getInstance()->removeModule("AS/contree_AS");
}

void ContreeAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayout();
    createRenderPipelineLayout();

    ShaderManager::getInstance()->removeMacro("CONTREE_GENERATION_FINISHED");
    ShaderManager::getInstance()->addModule("AS/contree_AS",
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
              m_Nodes = Generators::generateContree(
                  stoken, std::move(loader), p_GenerationInfo, m_Dimensions, m_UpdateBuffers);
          });
}

void ContreeAS::fromFile(std::filesystem::path path)
{
    p_FileThread.request_stop();
    p_FileThread = std::jthread([this, path](std::stop_token stoken) {
        p_Loading = true;
        Serializers::SerialInfo info;
        auto data = Serializers::loadContree(path);

        if (!data.has_value()) {
            return;
        }

        std::tie(info, m_Nodes) = data.value();

        m_Dimensions = info.dimensions;

        p_GenerationInfo.voxelCount = info.voxels;
        p_GenerationInfo.nodes = info.nodes;
        p_GenerationInfo.generationTime = 0;
        p_GenerationInfo.completionPercent = 1;

        m_UpdateBuffers = true;
        p_Loading = false;
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
        .hitDataAddress = p_Info.hitDataAddress,
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

void ContreeAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("AS/contree_AS"); }

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
                           .setShader("AS/contree_AS")
                           .setDebugName("Contree render pipeline")
                           .build();
}

void ContreeAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}
