#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <vulkan/vulkan_core.h>

#include "generators/common.hpp"

#include "../camera.hpp"
#include "loaders/loader.hpp"

#include <filesystem>

struct ASStructInfo {
    VkDevice device;
    VmaAllocator allocator;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueIndex;
    VkDescriptorPool descriptorPool;

    VkCommandPool commandPool;
    VkDescriptorSetLayout renderDescriptorLayout;
    VkDeviceAddress hitDataAddress;
};

enum class ModEnum : int {
    OP_SET = 1 << 0,
    OP_ERASE = 1 << 1,
};

struct ModInfo {
    alignas(16) int modType;
    alignas(16) glm::uvec3 voxelIndex;
    alignas(16) glm::vec3 colour;

    ModInfo(ModEnum mod, glm::uvec3 index, glm::vec3 colour)
        : modType(static_cast<int>(mod)), voxelIndex(index), colour(colour)
    {
    }
};

class IAccelerationStructure {
  public:
    IAccelerationStructure() { }
    virtual ~IAccelerationStructure() { }

    virtual void init(ASStructInfo info) { p_Info = info; }
    virtual void fromLoader(std::unique_ptr<Loader>&& loader) = 0;
    virtual void fromFile(std::filesystem::path path) { };
    virtual void render(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
        = 0;
    virtual void update(float dt) { }

    virtual void updateShaders() { }

    virtual void setVoxel(glm::uvec3 index, glm::vec3 colour)
    {
        p_Mods.emplace_back(ModEnum::OP_SET, index, colour);
    }

    virtual uint64_t getMemoryUsage() = 0;
    virtual uint64_t getTotalVoxels() { return p_GenerationInfo.voxelCount; }
    virtual uint64_t getNodes() { return p_GenerationInfo.nodes; }

    virtual bool isGenerating() { return p_Generating; }
    virtual float getGenerationCompletion() { return p_GenerationInfo.completionPercent; }
    virtual float getGenerationTime() { return p_GenerationInfo.generationTime; }

    virtual glm::uvec3 getDimensions() = 0;

    virtual bool isLoading() { return p_Loading; }

  protected:
    ASStructInfo p_Info;

    std::jthread p_GenerationThread;
    std::jthread p_FileThread;

    bool p_FinishedGeneration = false;
    bool p_Loading = false;
    bool p_UpdateBuffers = false;
    bool p_Generating = false;

    std::vector<ModInfo> p_Mods;

    Generators::GenerationInfo p_GenerationInfo;
};
