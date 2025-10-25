#pragma once

#include "accelerationStructures/accelerationStructure.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "events.hpp"

#include <memory>

enum class ASType : uint8_t {
    GRID = 0,
    MAX_TYPE,
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

    ASType m_CurrentType = ASType::GRID;
    std::unique_ptr<IAccelerationStructure> m_CurrentAS;
};
