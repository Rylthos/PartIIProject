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

    void init(VkDevice device, VmaAllocator allocator);
    void cleanup();

    void render(VkCommandBuffer cmd, uint32_t currentFrame);

    void setAS(ASType type);

  private:
    ASManager() { };

  private:
    VkDevice m_VkDevice;
    VmaAllocator m_VmaAllocator;

    std::unique_ptr<AccelerationStructure> m_CurrentAS;
};
