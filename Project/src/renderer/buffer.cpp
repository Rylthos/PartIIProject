#include "buffer.hpp"

#include "debug_utils.hpp"

#include "logger/logger.hpp"

#include <vulkan/vulkan_core.h>

Buffer::Buffer() { }
Buffer::~Buffer() { }

void Buffer::init(VkDevice device, VmaAllocator allocator, VkDeviceSize size,
    VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags, VmaMemoryUsage memoryUsage)
{
    m_Device = device;
    m_Allocator = allocator;
    m_Size = size;

    VkBufferCreateInfo bufferCI {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = m_Size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    VmaAllocationCreateInfo allocationCI { .flags = vmaFlags, .usage = memoryUsage };
    VK_CHECK(
        vmaCreateBuffer(m_Allocator, &bufferCI, &allocationCI, &m_Buffer, &m_Allocation, nullptr),
        "Failed to create buffer");
}

void Buffer::setDebugName(const char* name)
{
    Debug::setDebugName(m_Device, VK_OBJECT_TYPE_BUFFER, (uint64_t)m_Buffer, name);
}

void Buffer::cleanup()
{
    if (m_Buffer == VK_NULL_HANDLE)
        return;
    vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
    m_Buffer = VK_NULL_HANDLE;
}

void* Buffer::mapMemory()
{
    void* ptr;
    vmaMapMemory(m_Allocator, m_Allocation, &ptr);
    return ptr;
}

void Buffer::unmapMemory() { vmaUnmapMemory(m_Allocator, m_Allocation); }

void Buffer::copyToBuffer(VkCommandBuffer cmd, Buffer buffer, VkDeviceSize size,
    VkDeviceSize srcOffset, VkDeviceSize dstOffset)
{
    VkBufferCopy region {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size,
    };
    vkCmdCopyBuffer(cmd, m_Buffer, buffer.getBuffer(), 1, &region);
}

void Buffer::copyFromBuffer(VkCommandBuffer cmd, Buffer buffer, VkDeviceSize size,
    VkDeviceSize srcOffset, VkDeviceSize dstOffset)
{
    buffer.copyToBuffer(cmd, buffer, size, srcOffset, dstOffset);
}
