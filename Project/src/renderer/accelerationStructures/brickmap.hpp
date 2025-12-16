#pragma once

#include "acceleration_structure.hpp"
#include <stop_token>
#include <vulkan/vulkan_core.h>

#include "../buffer.hpp"
#include "generators/brickmap.hpp"

#include <vector>

class BrickmapAS : public IAccelerationStructure {

  public:
    BrickmapAS();
    ~BrickmapAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void fromFile(std::filesystem::path path) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override
    {
        return m_BrickgridBuffer.getSize() + m_BrickmapsBuffer.getSize() + m_ColourBuffer.getSize();
    }

    glm::uvec3 getDimensions() override { return m_BrickgridSize * 8u; }

  private:
    void createDescriptorLayout();
    void destroyDescriptorLayout();

    void createBuffers();
    void freeBuffers();

    void createDescriptorSet();
    void freeDescriptorSet();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();
    void createRenderPipeline();
    void destroyRenderPipeline();

    void createModPipelineLayout();
    void destroyModPipelineLayout();
    void createModPipeline();
    void destroyModPipeline();

    void createRequestPipelineLayout();
    void destroyRequestPipelineLayout();
    void createRequestPipeline();
    void destroyRequestPipeline();

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    VkPipelineLayout m_ModPipelineLayout;
    VkPipeline m_ModPipeline;

    VkPipelineLayout m_RequestPipelineLayout;
    VkPipeline m_RequestPipeline;

    Buffer m_BrickgridBuffer;
    Buffer m_BrickmapsBuffer;
    Buffer m_ColourBuffer;

    uint32_t m_Requests = 1024;
    Buffer m_RequestBuffer;
    uint32_t* m_MappedRequestBuffer = nullptr;

    glm::uvec3 m_BrickgridSize;

    std::vector<Generators::BrickgridPtr> m_Brickgrid;
    std::vector<Generators::Brickmap> m_Brickmaps;

    bool m_UpdateBuffers = false;
};
