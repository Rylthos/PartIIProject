#pragma once

#include <memory>

#include "network/node.hpp"

#include "accelerationStructures/acceleration_structure.hpp"

#include "events/events.hpp"

#include "buffer.hpp"
#include <queue>

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

inline static std::map<ASType, const char*> structTypeToStringMap {
    { ASType::GRID,     "Grid"     },
    { ASType::OCTREE,   "Octree"   },
    { ASType::CONTREE,  "Contree"  },
    { ASType::BRICKMAP, "Brickmap" },
    { ASType::TEXTURE,  "Texture"  },
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

    std::function<bool(const std::vector<uint8_t>&, uint32_t)> getHandleASChange()
    {
        using namespace std::placeholders;
        return std::bind(&ASManager::handleASChange, this, _1, _2);
    }

    std::function<bool(const std::vector<uint8_t>&, uint32_t)> getHandleUpdate()
    {
        using namespace std::placeholders;
        return std::bind(&ASManager::handleUpdate, this, _1, _2);
    }

    uint64_t getMemoryUsage();
    uint64_t getVoxels();
    uint64_t getNodes();

    glm::uvec3 getDimensions();

    bool animationEnabled();
    size_t getAnimationFrames();
    uint32_t getAnimationFrame();
    void setAnimationFrame(uint32_t target);

    bool finishedGeneration();

  private:
    ASManager() { };

    void UI(const Event& event);
    void mouse(const Event& event);

    bool handleASChange(const std::vector<uint8_t>& data, uint32_t messageID);
    bool handleUpdate(const std::vector<uint8_t>& data, uint32_t messageID);

  private:
    ASStructInfo m_InitInfo;

    RenderStyle m_CurrentRenderStyle = RenderStyle::NORMAL;

    ASType m_CurrentType = ASType::GRID;
    std::unique_ptr<IAccelerationStructure> m_CurrentAS;

    HitData* m_MappedHitData;
    Buffer m_HitDataBuffer;

    bool m_ShouldEdit;
    Modification::Type m_CurrentModification;
    std::optional<uint32_t> m_PressedButton;

    float m_PreviousPlacement = 0.f;

    std::queue<std::function<void()>> m_Functions;
};
