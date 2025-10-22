#pragma once

#include "accelerationStructures/accelerationStructure.hpp"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <memory>

struct ASManagerStructureInfo {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueIndex;
    VkDescriptorPool descriptorPool;
};

enum class ASType { GRID };

class ASManager {
  public:
    static ASManager* getManager()
    {
        static ASManager manager;
        return &manager;
    }

    void init(ASManagerStructureInfo initInfo);
    void cleanup();

    void render(VkCommandBuffer cmd, uint32_t currentFrame);

    void setAS(ASType type);

  private:
    ASManager() { };

  private:
    ASManagerStructureInfo m_InitInfo;

    ASType m_CurrentType = ASType::GRID;
    std::unique_ptr<IAccelerationStructure> m_CurrentAS;
};
