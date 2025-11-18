#pragma once

#include "vk_mem_alloc.h"
#include "vulkan/vulkan.h"
#include <vulkan/vulkan_core.h>

class Buffer {
  public:
    Buffer();
    ~Buffer();

    void init(VkDevice device, VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage,
        VmaAllocationCreateFlags vmaFlags, VmaMemoryUsage memoryUsage);

    void setDebugName(const char* name);

    void cleanup();

    VkDeviceSize getSize() { return m_Size; }
    VkBuffer getBuffer() { return m_Buffer; }

    void* mapMemory();
    void unmapMemory();

    void copyToBuffer(VkCommandBuffer cmd, Buffer buffer, VkDeviceSize size, VkDeviceSize srcOffset,
        VkDeviceSize dstOffset);
    void copyFromBuffer(VkCommandBuffer cmd, Buffer buffer, VkDeviceSize size,
        VkDeviceSize srcOffset, VkDeviceSize dstOffset);

  private:
    VkDevice m_Device;
    VmaAllocator m_Allocator;

    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VmaAllocation m_Allocation;
    VkDeviceSize m_Size = 0;
};
