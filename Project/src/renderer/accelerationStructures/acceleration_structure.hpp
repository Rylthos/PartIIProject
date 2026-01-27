#pragma once

#include "glm/fwd.hpp"
#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"

#include <vulkan/vulkan_core.h>

#include "generators/common.hpp"

#include "../camera.hpp"
#include "../modification_manager.hpp"
#include "../queue.hpp"
#include "../shader_manager.hpp"
#include "loaders/loader.hpp"
#include "modification/diff.hpp"

#include <filesystem>

#include "../network/node.hpp"

struct ASStructInfo {
    VkDevice device;
    VmaAllocator allocator;
    std::shared_ptr<Queue> graphicsQueue;
    VkDescriptorPool descriptorPool;

    VkCommandPool commandPool;
    VkDescriptorSetLayout renderDescriptorLayout;
    VkDeviceAddress hitDataAddress;

    Network::NetworkingInfo netInfo;
};

struct ModInfo {
    uint32_t shape;
    uint32_t type;
    uint32_t _1;
    uint32_t _2;
    alignas(16) glm::uvec3 voxelIndex;
    alignas(16) glm::vec3 colour;
    alignas(16) glm::vec4 additional;

    ModInfo(Modification::Shape shape, Modification::Type type, glm::uvec3 index, glm::vec3 colour,
        glm::vec4 additional)
        : shape(static_cast<int>(shape))
        , type(static_cast<int>(type))
        , voxelIndex(index)
        , colour(colour)
        , additional(additional)
    {
    }

    ModInfo(glm::uvec3 index, Modification::DiffType diff)
        : shape(static_cast<int>(Modification::Shape::VOXEL))
        , type(static_cast<int>(diff.first))
        , voxelIndex(index)
        , colour(diff.second)
        , additional(0.f)
    {
    }
};

class IAccelerationStructure {
  public:
    IAccelerationStructure() { }
    virtual ~IAccelerationStructure() { }

    virtual void init(ASStructInfo info) { p_Info = info; }

    virtual void fromLoader(std::unique_ptr<Loader>&& loader) = 0;
    virtual void fromRaw(const std::vector<uint8_t>& data) { }
    virtual void fromFile(std::filesystem::path path) { }

    virtual void render(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize)
        = 0;

    virtual void update(float dt) { }

    virtual void updateShaders() { }

    virtual void addMod(ModInfo mod) { p_Mods.push_back(mod); }

    virtual uint64_t getMemoryUsage() = 0;
    virtual uint64_t getTotalVoxels() { return p_GenerationInfo.voxelCount; }
    virtual uint64_t getNodes() { return p_GenerationInfo.nodes; }

    virtual bool isGenerating() { return p_Generating; }
    virtual float getGenerationCompletion() { return p_GenerationInfo.completionPercent; }
    virtual float getGenerationTime() { return p_GenerationInfo.generationTime; }

    virtual glm::uvec3 getDimensions() = 0;

    virtual bool isLoading() { return p_Loading; }

    virtual bool canAnimate() { return false; }
    virtual size_t getAnimationFrames() { return p_AnimationFrames.size(); }
    virtual uint32_t getAnimationFrame() { return p_CurrentFrame; }
    virtual void setAnimationFrame(uint32_t target) { p_TargetFrame = target; }

    virtual bool finishedGeneration() { return p_FinishedGeneration; }

  protected:
    void reset()
    {
        p_FinishedGeneration = false;
        ShaderManager::getInstance()->removeMacro("GENERATION_FINISHED");
        updateShaders();
    }

  protected:
    ASStructInfo p_Info;

    std::jthread p_GenerationThread;
    std::jthread p_FileThread;
    std::jthread p_RawThread;

    bool p_FinishedGeneration = false;
    bool p_Loading = false;
    bool p_UpdateBuffers = false;
    bool p_Generating = false;

    uint32_t p_CurrentFrame = 0;
    uint32_t p_TargetFrame = 0;
    Modification::AnimationFrames p_AnimationFrames;

    std::vector<ModInfo> p_Mods;

    Generators::GenerationInfo p_GenerationInfo;
};
