#pragma once

#include "vk_mem_alloc.h"
#include <vulkan/vulkan_core.h>

#include <string>

class Image {
  public:
    Image();
    ~Image();

    void init(VkDevice device, VmaAllocator allocator, uint32_t graphicsQueueIndex,
        VkExtent3D extent, VkFormat format, VkImageType type, VkImageUsageFlags usage,
        VmaAllocationCreateFlags flags = 0,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VkMemoryPropertyFlags memoryFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL);
    void createView(VkImageViewType type);
    void cleanup();

    static void transition(
        VkCommandBuffer cmd, VkImage image, VkImageLayout current, VkImageLayout target);

    void transition(VkCommandBuffer cmd, VkImageLayout current, VkImageLayout target);
    void copyToImage(VkCommandBuffer cmd, VkImage dst, VkExtent3D srcSize, VkExtent3D dstSize);

    void setDebugName(const std::string& name);
    void setDebugNameView(const std::string& name);

    VkImage getImage() const { return m_Image; }
    VkImageView getImageView() const { return m_View; }
    VkExtent3D getExtent() const { return m_Extent; }
    VkFormat getFormat() const { return m_Format; }
    VmaAllocation getAllocation() const { return m_Allocation; }

  private:
    VkDevice m_Device;
    VmaAllocator m_Allocator;

    VkImage m_Image = VK_NULL_HANDLE;
    VkImageView m_View = VK_NULL_HANDLE;
    VkFormat m_Format;
    VkExtent3D m_Extent;
    VmaAllocation m_Allocation;
};
