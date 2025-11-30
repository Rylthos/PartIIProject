#pragma once

#include "acceleration_structure.hpp"

#include "../buffer.hpp"
#include <vulkan/vulkan_core.h>

#include "generators/contree.hpp"

class ContreeAS : public IAccelerationStructure {
  public:
    ContreeAS();
    ~ContreeAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override { return m_ContreeBuffer.getSize(); }

  private:
    void createDescriptorLayout();
    void destroyDescriptorLayout();

    void createBuffers();
    void destroyBuffers();

    void createDescriptorSet();
    void freeDescriptorSet();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    glm::uvec3 m_Dimensions;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    std::vector<Generators::ContreeNode> m_Nodes;

    Buffer m_ContreeBuffer;

    bool m_UpdateBuffers = false;
};
