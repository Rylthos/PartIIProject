#include "octree.hpp"

#include <deque>

namespace Generators {
struct OctreeIntNode {
    glm::u8vec3 colour;
    bool visible;
    bool parent;
    uint8_t childMask;
    uint32_t childStartIndex = 0;
    uint32_t childCount = 0;
};

OctreeNode::OctreeNode(uint32_t ptr) { m_CurrentType = ptr; }

OctreeNode::OctreeNode(uint8_t childMask, uint32_t offset)
{
    m_CurrentType = NodeType {
        .flags = OCTREE_FLAG_EMPTY,
        .childMask = childMask,
        .offset = offset,
    };
}

OctreeNode::OctreeNode(uint8_t r, uint8_t g, uint8_t b)
{
    m_CurrentType = LeafType {
        .flags = OCTREE_FLAG_SOLID,
        .r = r,
        .g = g,
        .b = b,
    };
}

uint32_t OctreeNode::getData() const
{
    if (const NodeType* node = std::get_if<NodeType>(&m_CurrentType)) {
        uint32_t flags = (((uint32_t)node->flags & 0x3) << 30);
        assert(node->offset < 0x3FFFFF && "Incorrect pointer value given");
        uint32_t childMask = ((uint32_t)node->childMask & 0xFF) << 22;
        uint32_t offset = ((uint32_t)node->offset & 0x3FFFFF) << 0;
        return flags | childMask | offset;
    } else if (const LeafType* leaf = std::get_if<LeafType>(&m_CurrentType)) {
        uint32_t flags = ((uint32_t)leaf->flags & 0x3) << 30;
        uint32_t r = ((uint32_t)leaf->r) << 16;
        uint32_t g = ((uint32_t)leaf->g) << 8;
        uint32_t b = ((uint32_t)leaf->b) << 0;
        return flags | r | g | b;
    } else if (const uint32_t* ptr = std::get_if<uint32_t>(&m_CurrentType)) {
        return *ptr;
    } else {
        assert(false && "Not possible");
    }
}

static OctreeIntNode convert(const std::optional<glm::vec3> v)
{
    if (v.has_value()) {
        glm::vec3 colour = v.value();
        return OctreeIntNode {
            .colour = glm::u8vec3 { colour.x * 255, colour.y * 255, colour.z * 255 },
            .visible = 1,
            .parent = false,
            .childMask = 0,
            .childCount = 0,
        };
    } else
        return OctreeIntNode { .visible = false };
}

static std::optional<OctreeIntNode> allEqual(const std::array<OctreeIntNode, 8>& nodes)
{
    assert(nodes.size() == 8);
    if (nodes.at(0).parent)
        return std::optional<OctreeIntNode> {};

    bool allVisible = nodes.at(0).visible;
    glm::u8vec3 colour = nodes.at(0).colour;
    uint32_t count = 0;
    for (uint32_t i = 1; i < 8; i++) {
        if (nodes.at(i).parent) {
            return std::optional<OctreeIntNode> {};
        }

        if (nodes.at(i).visible != allVisible) {
            return std::optional<OctreeIntNode> {};
        }

        if (nodes.at(i).colour != colour) {
            return std::optional<OctreeIntNode> {};
        }

        count += nodes.at(i).childCount + 1;
    }

    return std::optional<OctreeIntNode>(OctreeIntNode {
        .colour = colour,
        .visible = allVisible,
        .parent = false,
        .childMask = 0,
        .childCount = count,
    });
};

void writeChildrenNodes(std::stop_token stoken, const std::vector<OctreeIntNode>& intNodes,
    size_t index, std::chrono::steady_clock clock,
    const std::chrono::steady_clock::time_point startTime, std::vector<OctreeNode>& nodes)
{
    if (stoken.stop_requested())
        return;

    size_t startingIndex = nodes.size();

    const OctreeIntNode& parentNode = intNodes.at(index);
    uint8_t childrenCount = glm::bitCount(parentNode.childMask);

    for (uint8_t i = 0; i < childrenCount; i++) {
        size_t childIndex = parentNode.childStartIndex - i;
        const OctreeIntNode childNode = intNodes.at(childIndex);

        nodes.push_back(OctreeNode(childNode.colour.r, childNode.colour.g, childNode.colour.b));
    }

    size_t currentOffset = 0;
    uint8_t farPointerCount = 0;
    for (uint8_t i = 0; i < childrenCount; i++) {
        size_t childIndex = parentNode.childStartIndex - i;
        const OctreeIntNode childNode = intNodes.at(childIndex);

        // Exceeds normal pointer with room for other nodes to add far pointers
        if (currentOffset >= 0x1F0000) {
            farPointerCount += 1;
            nodes.push_back(OctreeNode(0));
        }

        currentOffset += childNode.childCount + 1;
    }

    uint8_t currentFarPointer = 0;
    for (uint8_t i = 0; i < childrenCount; i++) {
        size_t childIndex = parentNode.childStartIndex - i;
        const OctreeIntNode childNode = intNodes.at(childIndex);

        if (childNode.parent) {
            size_t childStartingIndex = nodes.size();
            size_t offset = childStartingIndex - (startingIndex + i);
            writeChildrenNodes(stoken, intNodes, childIndex, clock, startTime, nodes);

            if (offset >= 0x200000) {
                size_t farPointerIndex = startingIndex + childrenCount + currentFarPointer;
                assert(childStartingIndex - farPointerIndex <= 0xFFFFFFFF);

                nodes[farPointerIndex] = OctreeNode(childStartingIndex - farPointerIndex);

                offset = 0x200000 + farPointerIndex - (startingIndex + i);
                currentFarPointer++;
            }

            nodes[startingIndex + i] = OctreeNode(childNode.childMask, offset);
        }
    }
    assert(farPointerCount >= currentFarPointer && "Pointers should match");
}

std::vector<OctreeNode> generateOctree(std::stop_token stoken, std::unique_ptr<Loader>&& loader,
    GenerationInfo& info, glm::uvec3& dimensions, bool& finished)
{
    std::chrono::steady_clock timer;

    dimensions = Loader::cubeDimensions(loader->getDimensionsDiv2());

    auto start = timer.now();

    uint64_t currentCode = 0;
    uint64_t finalCode = dimensions.x * dimensions.y * dimensions.z;

    const uint32_t maxDepth = 23;
    uint32_t currentDepth = maxDepth - 1;

    std::array<size_t, maxDepth> queueSizes;
    for (uint32_t i = 0; i < maxDepth; i++)
        queueSizes[i] = 0;

    std::array<std::array<OctreeIntNode, 8>, maxDepth> queues;
    std::vector<OctreeIntNode> intermediaryNodes;

    std::vector<OctreeNode> nodes;

    info.voxelCount = 0;

    while (currentCode != finalCode) {
        if (stoken.stop_requested())
            return nodes;

        const auto currentVoxel = loader->getVoxelMorton(currentCode);
        currentCode++;

        currentDepth = maxDepth - 1;
        queues[currentDepth][queueSizes[currentDepth]] = convert(currentVoxel);
        queueSizes[currentDepth]++;

        auto current = timer.now();

        std::chrono::duration<float, std::milli> difference = current - start;
        info.completionPercent = ((float)currentCode / (float)finalCode);
        info.generationTime = difference.count() / 1000.0f;

        while (currentDepth > 0 && queueSizes[currentDepth] == 8) {
            if (stoken.stop_requested())
                return nodes;

            OctreeIntNode node;

            const auto& possible_parent_node = allEqual(queues[currentDepth]);

            if (possible_parent_node.has_value()) {
                queues[currentDepth - 1][queueSizes[currentDepth - 1]]
                    = possible_parent_node.value();
                queueSizes[currentDepth - 1]++;
            } else {
                uint8_t childMask = 0;
                uint32_t childCount = 0;
                for (int8_t i = 0; i < 8; i++) {
                    if (queues[currentDepth].at(i).visible) {
                        childMask |= (1 << i);
                        intermediaryNodes.push_back(queues[currentDepth].at(i));
                        if (!queues[currentDepth].at(i).parent
                            && queues[currentDepth].at(i).visible) {
                            info.voxelCount += pow(8, 22 - currentDepth);
                        }
                        childCount += queues[currentDepth].at(i).childCount + 1;
                    }
                }

                OctreeIntNode parent = {
                    .colour = glm::u8vec3(1),
                    .visible = childMask != 0,
                    .parent = true,
                    .childMask = childMask,
                    .childStartIndex = (uint32_t)(intermediaryNodes.size() - 1),
                    .childCount = childCount,
                };
                queues[currentDepth - 1][queueSizes[currentDepth - 1]] = parent;
                queueSizes[currentDepth - 1]++;
            }

            queueSizes[currentDepth] = 0;
            currentDepth--;
        }
    }
    assert(queueSizes[currentDepth] == 1);
    intermediaryNodes.push_back(queues[currentDepth].at(0));
    if (!queues[currentDepth].at(0).parent && queues[currentDepth].at(0).visible) {
        info.voxelCount += pow(8, 22 - currentDepth);
    }

    nodes.reserve(intermediaryNodes.size());

    {
        const OctreeIntNode& finalNode = intermediaryNodes[intermediaryNodes.size() - 1];
        nodes.push_back(OctreeNode(finalNode.childMask, 1));
    }

    writeChildrenNodes(
        stoken, intermediaryNodes, intermediaryNodes.size() - 1, timer, start, nodes);

    auto end = timer.now();
    std::chrono::duration<float, std::milli> difference = end - start;

    info.generationTime = difference.count() / 1000.0f;
    info.completionPercent = 1.f;

    info.nodes = nodes.size();

    finished = true;

    return nodes;
}
}
