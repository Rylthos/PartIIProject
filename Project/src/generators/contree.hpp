#pragma once

#include <glm/glm.hpp>

#include <memory>
#include <thread>
#include <vector>

#include "../loaders/loader.hpp"
#include "common.hpp"

namespace Generators {
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

std::vector<ContreeNode> generateContree(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished);
}
