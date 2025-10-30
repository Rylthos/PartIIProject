#pragma once

#include "accelerationStructure.hpp"

#include "../buffer.hpp"

#include <variant>

class OctreeNode {
  public:
    OctreeNode(uint8_t childMask, uint32_t offset);
    OctreeNode(uint8_t r, uint8_t g, uint8_t b);

    uint32_t getData();

  private:
    enum OctreeFlags : uint8_t { // 2 bits
        OCTREE_FLAG_EMPTY = 0x00,
        OCTREE_FLAG_SOLID = 0x01,
    };

    struct NodeType {
        OctreeFlags flags;
        uint32_t childMask : 8;
        uint32_t offset    : 22;
    };
    struct LeafType {
        OctreeFlags flags;
        uint32_t r : 8;
        uint32_t g : 8;
        uint32_t b : 8;
    };
    std::variant<NodeType, LeafType> m_CurrentType;
};

class OctreeAS : public IAccelerationStructure {
  public:
    OctreeAS();
    ~OctreeAS();

    void init(ASStructInfo info) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet,
        VkExtent2D imageSize) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override;
    uint64_t getVoxels() override;

  private:
    void createDescriptorLayout();
    void destroyDescriptorLayout();

    void createBuffers();

    void createDescriptorSet();
    void freeDescriptorSet();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

  private:
    ASStructInfo m_Info;

    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    std::vector<OctreeNode> m_Nodes;

    Buffer m_StagingBuffer;
    Buffer m_OctreeBuffer;

    uint64_t m_VoxelCount = 0;
};
