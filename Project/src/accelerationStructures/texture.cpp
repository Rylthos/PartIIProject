#include "texture.hpp"

#include "../debug_utils.hpp"
#include "../shader_manager.hpp"
#include "acceleration_structure.hpp"

#include "../compute_pipeline.hpp"
#include "../descriptor_layout.hpp"
#include "../descriptor_set.hpp"
#include "../frame_commands.hpp"
#include "../pipeline_layout.hpp"

#include <cstring>
#include <vulkan/vulkan_core.h>

struct PushConstants {
    alignas(16) glm::vec3 cameraPosition;
    alignas(16) glm::uvec3 dimensions;
};

TextureAS::TextureAS() { }
TextureAS::~TextureAS()
{
    destroyImages();
    freeDescriptorSets();
    destroyDescriptorLayouts();
    destroyRenderPipeline();
    destroyRenderPipelineLayout();
    ShaderManager::getInstance()->removeModule("texture_AS");
}

void TextureAS::init(ASStructInfo info)
{
    IAccelerationStructure::init(info);

    createDescriptorLayouts();
    createRenderPipelineLayout();

    ShaderManager::getInstance()->removeMacro("TEXTURE_GENERATION_FINISHED");
    ShaderManager::getInstance()->addModule("texture_AS",
        std::bind(&TextureAS::createRenderPipeline, this),
        std::bind(&TextureAS::destroyRenderPipeline, this));

    createRenderPipeline();
}

void TextureAS::fromLoader(std::unique_ptr<Loader>&& loader)
{
    p_FinishedGeneration = false;

    ShaderManager::getInstance()->removeMacro("TEXTURE_GENERATION_FINISHED");
    updateShaders();

    p_GenerationThread.request_stop();

    p_Generating = true;
    p_GenerationThread
        = std::jthread([this, loader = std::move(loader)](std::stop_token stoken) mutable {
              generate(stoken, std::move(loader));
          });
}

void TextureAS::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
{
    Debug::beginCmdDebugLabel(cmd, "Texture AS render", { 0.0f, 0.0f, 1.0f, 1.0f });

    PushConstants pushConstant {
        .cameraPosition = camera.getPosition(),
        .dimensions = m_Dimensions,
    };

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipeline);
    std::vector<VkDescriptorSet> descriptorSets = {
        renderSet,
    };
    if (p_FinishedGeneration) {
        descriptorSets.push_back(m_ImageSet);
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_RenderPipelineLayout, 0,
        descriptorSets.size(), descriptorSets.data(), 0, nullptr);
    vkCmdPushConstants(cmd, m_RenderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
        sizeof(PushConstants), &pushConstant);

    vkCmdDispatch(cmd, std::ceil(imageSize.width / 8.f), std::ceil(imageSize.height / 8.f), 1);

    Debug::endCmdDebugLabel(cmd);
}

void TextureAS::update(float dt)
{
    if (m_UpdateBuffers) {
        destroyImages();
        freeDescriptorSets();

        ShaderManager::getInstance()->defineMacro("TEXTURE_GENERATION_FINISHED");
        updateShaders();

        createImages();
        createDescriptorSets();
        p_FinishedGeneration = true;
        m_UpdateBuffers = false;
        p_Generating = false;
    }
}

void TextureAS::updateShaders() { ShaderManager::getInstance()->moduleUpdated("texture_AS"); }

void TextureAS::createDescriptorLayouts()
{
    m_ImageSetLayout = DescriptorLayoutGenerator::start(p_Info.device)
                           .addStorageImageBinding(VK_SHADER_STAGE_COMPUTE_BIT, 0)
                           .setDebugName("Texture descriptor set layout")
                           .build();
}

void TextureAS::destroyDescriptorLayouts()
{
    vkDestroyDescriptorSetLayout(p_Info.device, m_ImageSetLayout, nullptr);
}

void TextureAS::createImages()
{
    VkExtent3D extent = { m_Dimensions.x, m_Dimensions.y, m_Dimensions.z };
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;

    m_DataImage.init(p_Info.device, p_Info.allocator, p_Info.graphicsQueueIndex, extent, format,
        VK_IMAGE_TYPE_3D, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    m_DataImage.setDebugName("Texture Data Image");

    m_DataImage.createView(VK_IMAGE_VIEW_TYPE_3D);
    m_DataImage.setDebugNameView("Texture Data Image View");

    VkDeviceSize imageSize = sizeof(GridVoxel) * m_Dimensions.x * m_Dimensions.y * m_Dimensions.z;

    auto bufferIndex = FrameCommands::getInstance()->createStaging(imageSize, [=, this](void* ptr) {
        uint8_t* data = (uint8_t*)ptr;
        for (size_t i = 0; i < m_Voxels.size(); i++) {
            data[i * 4 + 0] = m_Voxels[i].b;
            data[i * 4 + 1] = m_Voxels[i].g;
            data[i * 4 + 2] = m_Voxels[i].r;
            data[i * 4 + 3] = m_Voxels[i].a;
        }
    });

    FrameCommands::getInstance()->stagingEval(
        bufferIndex, [=, this](VkCommandBuffer cmd, FrameCommands::StagingBuffer buffer) {
            m_DataImage.transition(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            VkBufferImageCopy bufferImageCopy {
                .bufferOffset = buffer.offset,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = {
                    0, 0, 0,
                },
                .imageExtent = extent,
            };

            vkCmdCopyBufferToImage(cmd, buffer.buffer, m_DataImage.getImage(),
                VK_IMAGE_LAYOUT_GENERAL, 1, &bufferImageCopy);
        });
}

void TextureAS::destroyImages() { m_DataImage.cleanup(); }

void TextureAS::createDescriptorSets()
{
    m_ImageSet
        = DescriptorSetGenerator::start(p_Info.device, p_Info.descriptorPool, m_ImageSetLayout)
              .addImageDescriptor(0, m_DataImage, VK_IMAGE_LAYOUT_GENERAL)
              .setDebugName("Texture descriptor set")
              .build();
}

void TextureAS::freeDescriptorSets()
{
    if (m_ImageSet == VK_NULL_HANDLE)
        return;
    vkFreeDescriptorSets(p_Info.device, p_Info.descriptorPool, 1, &m_ImageSet);
}

void TextureAS::createRenderPipelineLayout()
{
    m_RenderPipelineLayout
        = PipelineLayoutGenerator::start(p_Info.device)
              .addDescriptorLayouts({ p_Info.renderDescriptorLayout, m_ImageSetLayout })
              .addPushConstant(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants))
              .setDebugName("Texture render pipeline layout")
              .build();
}

void TextureAS::destroyRenderPipelineLayout()
{
    vkDestroyPipelineLayout(p_Info.device, m_RenderPipelineLayout, nullptr);
}

void TextureAS::createRenderPipeline()
{
    m_RenderPipeline = ComputePipelineGenerator::start(p_Info.device, m_RenderPipelineLayout)
                           .setShader("texture_AS")
                           .setDebugName("texture render pipeline")
                           .build();
}

void TextureAS::destroyRenderPipeline()
{
    vkDestroyPipeline(p_Info.device, m_RenderPipeline, nullptr);
}

void TextureAS::generate(std::stop_token stoken, std::unique_ptr<Loader>&& loader)
{
    std::chrono::steady_clock timer;
    auto start = timer.now();

    m_Dimensions = loader->getDimensions();

    const size_t totalNodes = m_Dimensions.x * m_Dimensions.y * m_Dimensions.z;

    m_Voxels.resize(m_Dimensions.x * m_Dimensions.y * m_Dimensions.z);
    for (size_t z = 0; z < m_Dimensions.z; z++) {
        for (size_t y = 0; y < m_Dimensions.y; y++) {
            for (size_t x = 0; x < m_Dimensions.x; x++) {
                if (stoken.stop_requested())
                    return;

                size_t index = x + y * m_Dimensions.x + z * m_Dimensions.x * m_Dimensions.y;

                {
                    p_GenerationCompletion = (index + 1) / (float)totalNodes;

                    auto current = timer.now();
                    std::chrono::duration<float, std::milli> difference = current - start;
                    p_GenerationTime = difference.count() / 1000.0f;
                }

                auto v = loader->getVoxel({ x, y, z });

                glm::vec3 colour = v.has_value() ? v.value().colour : glm::vec3(0);
                m_Voxels[index] = glm::vec4 {
                    colour.r * 255,
                    colour.g * 255,
                    colour.b * 255,
                    v.has_value(),
                };
            }
        }
    }

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;
    p_GenerationTime = difference.count() / 1000.0f;

    m_UpdateBuffers = true;
}
