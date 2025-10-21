#pragma once

#include "accelerationStructure.hpp"
#include <vulkan/vulkan_core.h>

class GridAS : public AccelerationStructure {
  public:
    GridAS();
    ~GridAS();

    void init(VkDevice device, VmaAllocator allocator) override;
    void render(VkCommandBuffer cmd, uint32_t currentFrame) override;

  private:
    VkDevice p_VkDevice;
    VmaAllocator p_VmaAllocator;
};
