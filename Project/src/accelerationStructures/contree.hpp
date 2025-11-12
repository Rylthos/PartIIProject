#pragma once

#include "acceleration_structure.hpp"

#include "../buffer.hpp"
#include <algorithm>
#include <vulkan/vulkan_core.h>

#include <variant>

class ContreeNode {
  public:
    ContreeNode(uint64_t childMask, uint32_t offset, uint8_t r, uint8_t g, uint8_t b);
    ContreeNode(float r, float g, float b);

    std::array<uint64_t, 2> getData();

  private:
    enum ContreeFlags : uint8_t {
        CONTREE_FLAG_EMPTY = 0x00,
        CONTREE_FLAG_SOLID = 0x01,
    };

    struct NodeType {
        ContreeFlags flags;
        uint32_t colour : 24;
        uint32_t offset;
        uint64_t childMask;
    };

    struct LeafType {
        ContreeFlags flags;
        uint32_t _;
        uint32_t r;
        uint32_t g;
        uint32_t b;
    };
    std::variant<NodeType, LeafType> m_CurrentType;
};

class ContreeAS : public IAccelerationStructure {
    struct IntermediaryNode {
        glm::vec3 colour;
        bool visible;
        bool parent;
        uint64_t childMask;
        uint32_t childStartIndex = 0;
        uint32_t childCount = 0;
    };

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
    uint64_t getNodes() override { return m_Nodes.size(); }

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

    void generateNodes(std::stop_token stoken, std::unique_ptr<Loader> loader);

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    std::vector<ContreeNode> m_Nodes;

    Buffer m_StagingBuffer;
    Buffer m_ContreeBuffer;

    bool m_UpdateBuffers = false;
};
