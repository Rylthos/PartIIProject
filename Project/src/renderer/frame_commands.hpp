#pragma once

#include <cstdint>
#include <cstdlib>

#include <functional>

#include <unordered_map>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

class FrameCommands {

  public:
    using BufferIndex = size_t;
    struct StagingBuffer {
        size_t size;
        size_t offset;
        VkBuffer buffer;
    };

  public:
    static FrameCommands* getInstance();

    void init(VkDevice device, VmaAllocator allocator, VkQueue queue, uint32_t queueFamily);
    void cleanup();

    void commit();

    BufferIndex createStaging(size_t bufferSize, std::function<void(void*)> func);
    void stagingEval(BufferIndex staging, std::function<void(VkCommandBuffer, StagingBuffer)>);

  private:
    FrameCommands() { }

  private:
    VkDevice m_Device;
    VmaAllocator m_VmaAllocator;
    VkQueue m_Queue;

    VkCommandPool m_CommandPool;
    VkCommandBuffer m_CommandBuffer;

    VkFence m_CommitFence;

    size_t m_RequestedStagingSize = 0;
    std::vector<std::pair<size_t, std::function<void(void*)>>> m_CreateStagingFuncs;
    std::unordered_map<BufferIndex, std::function<void(VkCommandBuffer, StagingBuffer)>>
        m_StagingEvalFuncs;
};
