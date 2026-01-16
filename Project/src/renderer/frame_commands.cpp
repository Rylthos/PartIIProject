#include "frame_commands.hpp"

#include "logger/logger.hpp"

#include "buffer.hpp"
#include "debug_utils.hpp"

#include "glm/vector_relational.hpp"

#include <vulkan/vulkan_core.h>

FrameCommands* FrameCommands::getInstance()
{
    static FrameCommands frameCommands;
    return &frameCommands;
}

void FrameCommands::init(VkDevice device, VmaAllocator allocator, std::shared_ptr<Queue> queue)
{
    m_Device = device;
    m_VmaAllocator = allocator;
    m_Queue = queue;

    VkCommandPoolCreateInfo commandPoolCI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_Queue->getFamily(),
    };

    VK_CHECK(vkCreateCommandPool(m_Device, &commandPoolCI, nullptr, &m_CommandPool),
        "Failed to create command pool");
    Debug::setDebugName(
        m_Device, VK_OBJECT_TYPE_COMMAND_POOL, (uint64_t)m_CommandPool, "Frame command pool");

    VkCommandBufferAllocateInfo commandBufferAI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = m_CommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    vkAllocateCommandBuffers(m_Device, &commandBufferAI, &m_CommandBuffer);
    Debug::setDebugName(
        m_Device, VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)m_CommandBuffer, "Frame command buffer");

    VkFenceCreateInfo fenceCI {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VK_CHECK(vkCreateFence(m_Device, &fenceCI, nullptr, &m_CommitFence), "Failed to create fence");
}

void FrameCommands::cleanup()
{
    vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
    vkDestroyFence(m_Device, m_CommitFence, nullptr);
}

void FrameCommands::commit()
{
    if (m_RequestedStagingSize == 0) {
        return;
    }

    LOG_INFO("Frame command commit");

    Buffer stagingBuffer;
    stagingBuffer.init(m_Device, m_VmaAllocator, m_RequestedStagingSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST);
    stagingBuffer.setDebugName("Frame staging buffer");

    std::vector<size_t> offsets;
    offsets.reserve(m_CreateStagingFuncs.size());
    size_t currentOffset = 0;

    uint8_t* data = (uint8_t*)stagingBuffer.mapMemory();
    for (auto& pair : m_CreateStagingFuncs) {
        void* offset = (void*)(data + currentOffset);
        pair.second(offset);

        offsets.push_back(currentOffset);
        currentOffset += pair.first;
    }
    stagingBuffer.unmapMemory();

    VkCommandBufferBeginInfo commandBI {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };
    vkBeginCommandBuffer(m_CommandBuffer, &commandBI);

    for (auto& pair : m_StagingEvalFuncs) {
        const auto& stagingPair = m_CreateStagingFuncs[pair.first];
        StagingBuffer value = {
            .size = stagingPair.first,
            .offset = offsets[pair.first],
            .buffer = stagingBuffer.getBuffer(),
        };
        pair.second(m_CommandBuffer, value);
    }

    vkEndCommandBuffer(m_CommandBuffer);

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_CommandBuffer,
    };

    {
        std::lock_guard lock(m_Queue->getLock());
        VK_CHECK(vkQueueSubmit(m_Queue->getQueue(), 1, &submitInfo, m_CommitFence),
            "Failed to submit frame command");
    }

    vkWaitForFences(m_Device, 1, &m_CommitFence, VK_TRUE, 1e9);
    vkResetFences(m_Device, 1, &m_CommitFence);

    vkResetCommandBuffer(m_CommandBuffer, 0);

    m_CreateStagingFuncs.clear();
    m_StagingEvalFuncs.clear();
    m_RequestedStagingSize = 0;

    stagingBuffer.cleanup();
}

FrameCommands::BufferIndex FrameCommands::createStaging(
    size_t bufferSize, std::function<void(void*)> func)
{
    m_RequestedStagingSize += bufferSize;

    size_t index = m_CreateStagingFuncs.size();
    m_CreateStagingFuncs.push_back(std::make_pair(bufferSize, func));

    return index;
}

void FrameCommands::stagingEval(FrameCommands::BufferIndex stagingIndex,
    std::function<void(VkCommandBuffer, StagingBuffer)> func)
{
    m_StagingEvalFuncs[stagingIndex] = func;
}
