#pragma once

#include "acceleration_structure.hpp"
#include <vulkan/vulkan_core.h>

class BrickmapAS : public IAccelerationStructure {
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

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;
};
