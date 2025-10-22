#pragma once

#include "accelerationStructures/accelerationStructure.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <memory>

enum class ASType { GRID };

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

  private:
    ASManager() { };

  private:
    ASStructInfo m_InitInfo;

    ASType m_CurrentType = ASType::GRID;
    std::unique_ptr<IAccelerationStructure> m_CurrentAS;
};
