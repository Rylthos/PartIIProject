#pragma once

#include <memory>

#include "accelerationStructures/acceleration_structure.hpp"

#include "events/events.hpp"

#include "buffer.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

enum class ASType : uint8_t {
    GRID = 0,
    TEXTURE = 1,
    OCTREE = 2,
    CONTREE = 3,
    BRICKMAP = 4,
    MAX_TYPE,
};

enum class RenderStyle : uint8_t {
    NORMAL = 0,
    HEAT = 1,
    CYCLES = 2,
    MAX_STYLE,
};

struct HitData {
    alignas(16) glm::vec4 hitPosition;
    alignas(16) glm::ivec4 voxelIndex;
    alignas(16) glm::vec4 normal;
    alignas(16) int hit;
};

class ASManager {
  public:
    static ASManager* getManager()
    {
        static ASManager manager;
        return &manager;
    }

    void init(ASStructInfo initInfo);
    void cleanup();

    void render(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize);
    void update(float dt);

    void setAS(ASType type);
    void loadAS(
        std::filesystem::path, bool validStructures[static_cast<uint8_t>(ASType::MAX_TYPE)]);

    void updateShaders();

    std::function<void(const Event& event)> getUIEvent()
    {
        return std::bind(&ASManager::UI, this, std::placeholders::_1);
    }

    std::function<void(const Event& event)> getMouseEvent()
    {
        return std::bind(&ASManager::mouse, this, std::placeholders::_1);
    }

    uint64_t getMemoryUsage() { return m_CurrentAS->getMemoryUsage(); }
    uint64_t getVoxels() { return m_CurrentAS->getTotalVoxels(); }
    uint64_t getNodes() { return m_CurrentAS->getNodes(); }

    glm::uvec3 getDimensions() { return m_CurrentAS->getDimensions(); };

  private:
    ASManager() { };

    void UI(const Event& event);
    void mouse(const Event& event);

  private:
    ASStructInfo m_InitInfo;

    RenderStyle m_CurrentRenderStyle = RenderStyle::NORMAL;

    ASType m_CurrentType = ASType::GRID;
    std::unique_ptr<IAccelerationStructure> m_CurrentAS;

    HitData* m_MappedHitData;
    Buffer m_HitDataBuffer;
};
