#pragma once

#include "acceleration_structure.hpp"

#include "../buffer.hpp"

#include <chrono>
#include <variant>
#include <vulkan/vulkan_core.h>

#include "../generators/octree.hpp"

class OctreeAS : public IAccelerationStructure {

  public:
    OctreeAS();
    ~OctreeAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override { return m_OctreeBuffer.getSize(); }
    uint64_t getNodes() override { return m_Nodes.size(); }

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
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    glm::uvec3 m_Dimensions;

    std::vector<Generators::OctreeNode> m_Nodes;

    Buffer m_OctreeBuffer;

    bool m_UpdateBuffers = false;
};
