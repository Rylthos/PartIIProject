#pragma once

#include "acceleration_structure.hpp"
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
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override
    {
        return m_OccupancyBuffer.getSize() + m_ColourBuffer.getSize();
    }
    uint64_t getNodes() override { return m_Voxels.size(); }
    uint64_t getTotalVoxels() override { return m_Voxels.size(); }

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

    void generate(std::stop_token stoken, std::unique_ptr<Loader>&& loader);

  private:
    std::vector<GridVoxel> m_Voxels;
    glm::uvec3 m_Dimensions;

    Buffer m_OccupancyBuffer;
    Buffer m_ColourBuffer;

    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;

    bool m_UpdateBuffers = false;
};
