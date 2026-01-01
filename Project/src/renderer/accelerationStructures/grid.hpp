#pragma once

#include "acceleration_structure.hpp"
#include <vulkan/vulkan_core.h>

#include "generators/grid.hpp"

#include "../buffer.hpp"
#include "glm/fwd.hpp"

#include <glm/glm.hpp>

class GridAS : public IAccelerationStructure {
  public:
    GridAS();
    ~GridAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void fromFile(std::filesystem::path path) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override
    {
        return m_OccupancyBuffer.getSize() + m_ColourBuffer.getSize();
    }

    glm::uvec3 getDimensions() override { return m_Dimensions; }

    bool canAnimate() override { return true; }

  private:
    void createDescriptorLayouts();
    void destroyDescriptorLayouts();

    void createBuffer();
    void freeBuffers();

    void createDescriptorSets();
    void freeDescriptorSets();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

    void createModPipelineLayout();
    void destroyModPipelineLayout();

    void createModPipeline();
    void destroyModPipeline();

  private:
    std::vector<Generators::GridVoxel> m_Voxels;
    glm::uvec3 m_Dimensions;

    Buffer m_OccupancyBuffer;
    Buffer m_ColourBuffer;

    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;

    VkPipeline m_ModPipeline;
    VkPipelineLayout m_ModPipelineLayout;

    bool m_UpdateBuffers = false;
};
