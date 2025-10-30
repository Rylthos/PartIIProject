#pragma once

#include "accelerationStructure.hpp"
#include <vulkan/vulkan_core.h>

#include <array>

#include "../buffer.hpp"

#include <glm/glm.hpp>

#define GRID_DIMENSIONS 32

class GridAS : public IAccelerationStructure {
    struct Voxel {
        glm::vec3 colour;
        bool visible;
    };

  public:
    GridAS();
    ~GridAS();

    void init(ASStructInfo info) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet,
        VkExtent2D imageSize) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override;
    uint64_t getVoxels() override;

  private:
    void createDescriptorLayouts();
    void destroyDescriptorLayouts();

    void createBuffer();

    void createDescriptorSets();
    void freeDescriptorSets();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

  private:
    std::array<Voxel, GRID_DIMENSIONS * GRID_DIMENSIONS * GRID_DIMENSIONS> m_Voxels;

    ASStructInfo m_Info;

    Buffer m_OccupancyBuffer;
    Buffer m_ColourBuffer;
    Buffer m_StagingBuffer;

    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;
};
