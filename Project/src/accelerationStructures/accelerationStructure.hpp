#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
#include <vulkan/vulkan_core.h>

#include "../camera.hpp"
#include "../loaders/loader.hpp"

struct ASStructInfo {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueIndex;
    VkDescriptorPool descriptorPool;

    VkCommandPool commandPool;
    VkDescriptorSetLayout drawImageDescriptorLayout;
};

class IAccelerationStructure {
  public:
    IAccelerationStructure() { }
    virtual ~IAccelerationStructure() { }

    virtual void init(ASStructInfo info) = 0;
    virtual void fromLoader(Loader& loader) = 0;
    virtual void render(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
        = 0;

    virtual void updateShaders() { }

    virtual uint64_t getMemoryUsage() = 0;
    virtual uint64_t getVoxels() = 0;

  protected:
  private:
};
