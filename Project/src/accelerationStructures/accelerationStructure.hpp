#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

struct ASStructInfo {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueIndex;
    VkDescriptorPool descriptorPool;
};

class IAccelerationStructure {
  public:
    IAccelerationStructure() { }
    ~IAccelerationStructure() { }

    virtual void init(ASStructInfo info) { }
    virtual void render(VkCommandBuffer cmd, uint32_t currentFrame) { }

  protected:
  private:
};
