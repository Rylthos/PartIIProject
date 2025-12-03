#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "common.hpp"
#include "loaders/loader.hpp"

namespace Generators {
class OctreeNode {
  public:
    OctreeNode(uint32_t offset);
    OctreeNode(uint8_t childMask, uint32_t offset);
    OctreeNode(uint8_t r, uint8_t g, uint8_t b);

    uint32_t getData() const;

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

std::vector<OctreeNode> generateOctree(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished);
}
