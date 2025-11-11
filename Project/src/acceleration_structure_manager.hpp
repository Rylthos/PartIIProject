#pragma once

#include "accelerationStructures/accelerationStructure.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "events.hpp"

#include <memory>

enum class ASType : uint8_t {
    GRID = 0,
    OCTREE = 1,
    CONTREE = 2,
    MAX_TYPE,
};

enum class RenderStyle : uint8_t {
    NORMAL = 0,
    HEAT = 1,
    CYCLES = 2,
    MAX_STYLE,
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
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize);
    void update(float dt);

    void setAS(ASType type);

    void updateShaders();

    std::function<void(const Event& event)> getUIEvent()
    {
        return std::bind(&ASManager::UI, this, std::placeholders::_1);
    }

  private:
    ASManager() { };

    void UI(const Event& event);

  private:
    ASStructInfo m_InitInfo;

    RenderStyle m_CurrentRenderStyle = RenderStyle::NORMAL;

    ASType m_CurrentType = ASType::CONTREE;
    std::unique_ptr<IAccelerationStructure> m_CurrentAS;
};
