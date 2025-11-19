#pragma once

#include "acceleration_structure.hpp"
#include <stop_token>
#include <vulkan/vulkan_core.h>

#include "../buffer.hpp"

#include <vector>

class BrickmapAS : public IAccelerationStructure {
    using BrickgridPtr = uint32_t; // Highest bit marks loaded

    struct Brickmap {
        uint64_t colourPtr;
        uint64_t occupancy[8];
        std::vector<uint8_t> colour;
    };

  public:
    BrickmapAS();
    ~BrickmapAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override { return 0; }
    uint64_t getNodes() override { return 0; }

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

    void generate(std::stop_token stoken, std::unique_ptr<Loader> loader);

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    Buffer m_BrickgridBuffer;
    Buffer m_BrickmapsBuffer;
    Buffer m_ColourBuffer;

    glm::uvec3 m_BrickgridSize;

    std::vector<BrickgridPtr> m_Brickgrid;
    std::vector<Brickmap> m_Brickmaps;

    bool m_UpdateBuffers = false;
};
