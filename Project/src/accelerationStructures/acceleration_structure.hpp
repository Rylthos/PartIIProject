#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <vulkan/vulkan_core.h>

#include "../generators/common.hpp"

#include "../camera.hpp"
#include "../loaders/loader.hpp"

struct ASStructInfo {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueIndex;
    VkDescriptorPool descriptorPool;

    VkCommandPool commandPool;
    VkDescriptorSetLayout renderDescriptorLayout;
};

class IAccelerationStructure {
  public:
    IAccelerationStructure() { }
    virtual ~IAccelerationStructure() { }

    virtual void init(ASStructInfo info) { p_Info = info; }
    virtual void fromLoader(std::unique_ptr<Loader>&& loader) = 0;
    virtual void render(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
        = 0;
    virtual void update(float dt) { }

    virtual void updateShaders() { }

    virtual uint64_t getMemoryUsage() = 0;
    virtual uint64_t getTotalVoxels() { return p_GenerationInfo.voxelCount; }
    virtual uint64_t getNodes() { return p_GenerationInfo.nodes; }

    virtual bool isGenerating() { return p_Generating; }
    virtual float getGenerationCompletion() { return p_GenerationInfo.completionPercent; }
    virtual float getGenerationTime() { return p_GenerationInfo.generationTime; }

  protected:
    ASStructInfo p_Info;

    std::jthread p_GenerationThread;
    bool p_FinishedGeneration = false;
    bool p_UpdateBuffers = false;
    bool p_Generating = false;

    Generators::GenerationInfo p_GenerationInfo;
};
