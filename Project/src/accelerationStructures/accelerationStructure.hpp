#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

class AccelerationStructure {
  public:
    AccelerationStructure() { }
    ~AccelerationStructure() { }

    virtual void init(VkDevice device, VmaAllocator allocator) { }
    virtual void render(VkCommandBuffer cmd, uint32_t currentFrame) { }

  protected:
  private:
};
