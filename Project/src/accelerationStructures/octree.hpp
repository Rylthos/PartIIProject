#pragma once

#include "acceleration_structure.hpp"

#include "../buffer.hpp"

#include <algorithm>
#include <chrono>
#include <variant>
#include <vulkan/vulkan_core.h>

class OctreeNode {
  public:
    OctreeNode(uint32_t offset);
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
    std::variant<NodeType, LeafType, uint32_t> m_CurrentType;
};

class OctreeAS : public IAccelerationStructure {
    struct IntermediaryNode {
        glm::u8vec3 colour;
        bool visible;
        bool parent;
        uint8_t childMask;
        uint32_t childStartIndex = 0;
        uint32_t childCount = 0;
    };

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

    void generateNodes(std::stop_token stoken, std::unique_ptr<Loader> loader);
    void writeChildrenNodes(std::stop_token stoken, const std::vector<IntermediaryNode>& nodes,
        size_t index, std::chrono::steady_clock clock,
        const std::chrono::steady_clock::time_point startTime);

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    std::vector<OctreeNode> m_Nodes;

    Buffer m_OctreeBuffer;

    bool m_UpdateBuffers = false;
};
