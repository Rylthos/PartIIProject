#include "contree.hpp"

#include <deque>
#include <iterator>

namespace Generators {
struct ContreeIntNode {
    glm::vec3 colour;
    bool visible;
    bool parent;
    uint64_t childMask;
    uint32_t childStartIndex = 0;
    uint32_t childCount = 0;
};

ContreeNode::ContreeNode(uint64_t childMask, uint32_t offset, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t colour = (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b;
    m_CurrentType = NodeType {
        .flags = CONTREE_FLAG_EMPTY,
        .colour = colour,
        .offset = offset,
        .childMask = childMask,
    };
}

ContreeNode::ContreeNode(float r, float g, float b)
{
    uint32_t rI = (uint32_t)(std::clamp(r, 0.f, 1.f) * (float)0xFFFF);
    uint32_t gI = (uint32_t)(std::clamp(g, 0.f, 1.f) * (float)0xFFFF);
    uint32_t bI = (uint32_t)(std::clamp(b, 0.f, 1.f) * (float)0xFFFF);
    m_CurrentType = LeafType {
        .flags = CONTREE_FLAG_SOLID,
        .r = rI,
        .g = gI,
        .b = bI,
    };
}

std::array<uint64_t, 2> ContreeNode::getData()
{
    std::array<uint64_t, 2> data;
    if (const NodeType* node = std::get_if<NodeType>(&m_CurrentType)) {
        uint64_t flags = ((uint64_t)node->flags) << 56;
        uint64_t colour = ((uint64_t)node->colour) << 32;
        uint64_t offset = node->offset;
        uint64_t childMask = node->childMask;
        data[0] = flags | colour | offset;
        data[1] = childMask;
    } else if (const LeafType* leaf = std::get_if<LeafType>(&m_CurrentType)) {
        uint64_t flags = ((uint64_t)leaf->flags) << 56;
        uint64_t r = (uint64_t)leaf->r;
        uint64_t g = (uint64_t)leaf->g << 32;
        uint64_t b = (uint64_t)leaf->b;

        data[0] = flags | r;
        data[1] = g | b;
    }

    return data;
}

static ContreeIntNode convert(const std::optional<glm::vec3> v)
{
    if (v.has_value()) {
        glm::vec3 colour = v.value();
        return ContreeIntNode {
            .colour = colour,
            .visible = true,
            .parent = false,
            .childMask = 0,
        };
    } else {
        return ContreeIntNode {
            .visible = false,
        };
    }
};

static std::optional<ContreeIntNode> allEqual(const std::deque<ContreeIntNode>& nodes)
{
    assert(nodes.size() == 64);
    bool allVisible = nodes.at(0).visible;
    glm::vec3 colour = nodes.at(0).colour;
    uint32_t count = 0;

    for (uint32_t i = 0; i < 64; i++) {
        if (nodes.at(i).parent || nodes.at(i).visible != allVisible
            || nodes.at(i).colour != colour) {
            return std::optional<ContreeIntNode> {};
        }
        count += nodes.at(i).childCount + 1;
    }

    return std::optional<ContreeIntNode>(ContreeIntNode {
        .colour = colour,
        .visible = allVisible,
        .parent = false,
        .childMask = 0,
        .childCount = count,
    });
};

std::vector<ContreeNode> generateContree(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished)
{
    std::chrono::steady_clock timer;

    dimensions = loader->getDimensionsDiv4();

    auto start = timer.now();

    uint64_t currentCode = 0;

    uint64_t finalCode = dimensions.x * dimensions.y * dimensions.z;

    const uint32_t maxDepth = 11;
    uint32_t currentDepth = maxDepth;

    std::array<std::deque<ContreeIntNode>, maxDepth> queues;
    std::vector<ContreeIntNode> intermediaryNodes;

    std::vector<ContreeNode> nodes;

    info.voxelCount = 0;

    while (currentCode != finalCode) {
        if (stoken.stop_requested())
            return nodes;

        const auto currentVoxel = loader->getVoxelMorton2(currentCode);
        currentCode++;

        currentDepth = maxDepth - 1;
        queues[currentDepth].push_back(convert(currentVoxel));

        auto current = timer.now();
        std::chrono::duration<float, std::milli> difference = current - start;
        info.completionPercent = ((float)currentCode / (float)finalCode);
        info.generationTime = difference.count() / 1000.0f;

        while (currentDepth > 0 && queues[currentDepth].size() == 64) {
            if (stoken.stop_requested())
                return nodes;

            ContreeIntNode node;

            const auto& possibleParentNode = allEqual(queues[currentDepth]);

            if (possibleParentNode.has_value()) {
                queues[currentDepth - 1].push_back(possibleParentNode.value());
            } else {
                uint64_t childMask = 0;
                uint32_t childCount = 0;
                for (int8_t i = 63; i >= 0; i--) {
                    if (queues[currentDepth].at(i).visible) {
                        childMask |= (1ull << i);
                        intermediaryNodes.push_back(queues[currentDepth].at(i));
                        if (!queues[currentDepth].at(i).parent) {
                            info.voxelCount += pow(64, 10 - currentDepth);
                        }
                        childCount += queues[currentDepth].at(i).childCount + 1;
                    }
                }

                ContreeIntNode parent = {
                    .visible = childMask != 0,
                    .parent = true,
                    .childMask = childMask,
                    .childStartIndex = (uint32_t)(intermediaryNodes.size() - 1),
                    .childCount = childCount,
                };

                queues[currentDepth - 1].push_back(parent);
            }
            queues[currentDepth].clear();
            currentDepth--;
        }
    }
    assert(queues[currentDepth].size() == 1);
    intermediaryNodes.push_back(queues[currentDepth].at(0));
    if (!queues[currentDepth].at(0).parent && queues[currentDepth].at(0).visible) {
        info.voxelCount += pow(64, 10 - currentDepth);
    }

    nodes.reserve(intermediaryNodes.size());

    size_t index = intermediaryNodes.size() - 1;
    for (auto it = intermediaryNodes.rbegin(); it != intermediaryNodes.rend(); it++) {
        if (stoken.stop_requested())
            return nodes;

        if (it->parent) {
            glm::vec3 c = it->colour * 255.f;
            glm::u8vec3 colour = glm::u8vec3(c.r, c.g, c.b);
            uint32_t targetOffset = index - it->childStartIndex;
            nodes.push_back(ContreeNode(it->childMask, targetOffset, colour.r, colour.g, colour.b));
        } else if (it->visible) {
            nodes.push_back(ContreeNode(it->colour.r, it->colour.g, it->colour.b));
        }
        index--;
    }

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;
    info.generationTime = difference.count() / 1000.0f;
    info.completionPercent = 1.f;

    info.nodes = nodes.size();

    finished = true;

    return nodes;
}

}
