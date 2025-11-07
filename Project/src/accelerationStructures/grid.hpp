#pragma once

#include "accelerationStructure.hpp"
#include <vulkan/vulkan_core.h>

#include <array>

#include "../buffer.hpp"
#include "../voxel.hpp"
#include "glm/fwd.hpp"

#include <glm/glm.hpp>

class GridAS : public IAccelerationStructure {
    struct GridVoxel {
        bool visible;
        glm::vec3 colour;
    };

  public:
    GridAS();
    ~GridAS();

    void init(ASStructInfo info) override;
    void fromLoader(Loader& loader) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet,
        VkExtent2D imageSize) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override;
    uint64_t getStoredVoxels() override;
    uint64_t getTotalVoxels() override;

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

  private:
    std::vector<GridVoxel> m_Voxels;
    glm::uvec3 m_Dimensions;

    ASStructInfo m_Info;

    Buffer m_OccupancyBuffer;
    Buffer m_ColourBuffer;
    Buffer m_StagingBuffer;

    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;
};
