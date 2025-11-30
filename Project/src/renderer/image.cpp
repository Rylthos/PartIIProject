#include "image.hpp"

#include "debug_utils.hpp"

#include "logger/logger.hpp"

#include <vulkan/vulkan_core.h>

Image::~Image() { }
Image::Image() { }

void Image::init(VkDevice device, VmaAllocator allocator, uint32_t graphicsQueueIndex,
    VkExtent3D extent, VkFormat format, VkImageType type, VkImageUsageFlags usage)
{
    m_Device = device;
    m_Allocator = allocator;

    m_Extent = extent;
    m_Format = format;

    VkImageCreateInfo imageCI {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = type,
        .format = m_Format,
        .extent = m_Extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &graphicsQueueIndex,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VmaAllocationCreateInfo allocationCI {
        .flags = 0,
        .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VK_CHECK(vmaCreateImage(m_Allocator, &imageCI, &allocationCI, &m_Image, &m_Allocation, nullptr),
        "Failed to allocate voxel draw image");
}

void Image::createView(VkImageViewType type)
{
    VkImageViewCreateInfo imageViewCI {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = m_Image,
        .viewType = type,
        .format = m_Format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VK_CHECK(vkCreateImageView(m_Device, &imageViewCI, nullptr, &m_View),
        "Failed to create image views");
}

void Image::cleanup()
{
    if (m_Image == VK_NULL_HANDLE)
        return;

    if (m_View != VK_NULL_HANDLE)
        vkDestroyImageView(m_Device, m_View, nullptr);

    vmaDestroyImage(m_Allocator, m_Image, m_Allocation);
}

void Image::transition(
    VkCommandBuffer cmd, VkImage image, VkImageLayout current, VkImageLayout target)
{
    VkImageMemoryBarrier2 imageBarrier {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    imageBarrier.pNext = nullptr;
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    imageBarrier.oldLayout = current;
    imageBarrier.newLayout = target;
    imageBarrier.image = image;
    imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange.baseMipLevel = 0;
    imageBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    imageBarrier.subresourceRange.baseArrayLayer = 0;
    imageBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dependencyInfo {};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pNext = nullptr;
    dependencyInfo.dependencyFlags = 0;
    dependencyInfo.memoryBarrierCount = 0;
    dependencyInfo.pMemoryBarriers = nullptr;
    dependencyInfo.bufferMemoryBarrierCount = 0;
    dependencyInfo.pBufferMemoryBarriers = nullptr;
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);
}

void Image::transition(VkCommandBuffer cmd, VkImageLayout current, VkImageLayout target)
{
    Image::transition(cmd, m_Image, current, target);
}

void Image::copyToImage(VkCommandBuffer cmd, VkImage dst, VkExtent3D srcSize, VkExtent3D dstSize)
{
    VkImageBlit2 blitRegion {};
    blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
    blitRegion.pNext = nullptr;
    blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.srcSubresource.mipLevel = 0;
    blitRegion.srcSubresource.baseArrayLayer = 0;
    blitRegion.srcSubresource.layerCount = 1;
    blitRegion.srcOffsets[1].x = srcSize.width;
    blitRegion.srcOffsets[1].y = srcSize.height;
    blitRegion.srcOffsets[1].z = srcSize.depth;
    blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blitRegion.dstSubresource.mipLevel = 0;
    blitRegion.dstSubresource.baseArrayLayer = 0;
    blitRegion.dstSubresource.layerCount = 1;
    blitRegion.dstOffsets[1].x = dstSize.width;
    blitRegion.dstOffsets[1].y = dstSize.height;
    blitRegion.dstOffsets[1].z = dstSize.depth;

    VkBlitImageInfo2 blitInfo {};
    blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blitInfo.pNext = nullptr;
    blitInfo.srcImage = m_Image;
    blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blitInfo.dstImage = dst;
    blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    blitInfo.regionCount = 1;
    blitInfo.pRegions = &blitRegion;
    blitInfo.filter = VK_FILTER_LINEAR;

    vkCmdBlitImage2(cmd, &blitInfo);
}

void Image::setDebugName(const std::string& name)
{
    Debug::setDebugName(m_Device, VK_OBJECT_TYPE_IMAGE, (uint64_t)m_Image, name.c_str());
}

void Image::setDebugNameView(const std::string& name)
{
    Debug::setDebugName(m_Device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)m_View, name.c_str());
}
