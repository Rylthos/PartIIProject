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

    virtual void init(ASStructInfo info) { p_Info = info; }
    virtual void fromLoader(std::unique_ptr<Loader>&& loader) = 0;
    virtual void render(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
        = 0;
    virtual void update(float dt) { }

    virtual void updateShaders() { }

    virtual uint64_t getMemoryUsage() = 0;
    virtual uint64_t getTotalVoxels() { return p_VoxelCount; }
    virtual uint64_t getNodes() = 0;

    virtual bool isGenerating() { return p_Generating; }
    virtual float getGenerationCompletion() { return p_GenerationCompletion; }
    virtual float getGenerationTime() { return p_GenerationTime; }

  protected:
    ASStructInfo p_Info;

    uint64_t p_VoxelCount = 0;

    std::jthread p_GenerationThread;
    bool p_FinishedGeneration = false;
    bool p_UpdateBuffers = false;
    bool p_Generating = false;

    float p_GenerationCompletion = 0.f;
    float p_GenerationTime = 0.f;

  private:
};
