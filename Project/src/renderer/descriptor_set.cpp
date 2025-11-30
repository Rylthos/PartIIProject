#include "descriptor_set.hpp"

#include <vulkan/vulkan_core.h>

#include "debug_utils.hpp"

DescriptorSetGenerator DescriptorSetGenerator::start(
    VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    return DescriptorSetGenerator(device, pool, layout);
}

DescriptorSetGenerator& DescriptorSetGenerator::addBufferDescriptor(
    uint32_t binding, Buffer& buffer, uint32_t offset)
{
    m_Buffers.push_back({
        .buffer = buffer,
        .binding = binding,
        .offset = offset,
        .types = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    });

    return *this;
}

DescriptorSetGenerator& DescriptorSetGenerator::addImageDescriptor(
    uint32_t binding, Image& image, VkImageLayout layout)
{
    m_Images.push_back({
        .image = image,
        .binding = binding,
        .types = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    });

    return *this;
}

DescriptorSetGenerator& DescriptorSetGenerator::setDebugName(const char* name)
{
    m_DebugName = std::string(name);

    return *this;
}

VkDescriptorSet DescriptorSetGenerator::build()
{
    VkDescriptorSet set;

    VkDescriptorSetAllocateInfo setAI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = m_DescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &m_Layout,
    };
    VK_CHECK(
        vkAllocateDescriptorSets(m_Device, &setAI, &set), "Failed to allocate descriptor sets");

    std::vector<VkWriteDescriptorSet> writeDescriptors;

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    bufferInfos.resize(m_Buffers.size());

    for (auto& buffer : m_Buffers) {
        bufferInfos.push_back({
            .buffer = buffer.buffer.getBuffer(),
            .offset = buffer.offset,
            .range = buffer.buffer.getSize(),
        });

        writeDescriptors.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = set,
            .dstBinding = buffer.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = buffer.types,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfos.at(bufferInfos.size() - 1),
            .pTexelBufferView = nullptr,
        });
    }

    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.resize(m_Buffers.size());

    for (auto& image : m_Images) {
        imageInfos.push_back({
            .sampler = VK_NULL_HANDLE,
            .imageView = image.image.getImageView(),
            .imageLayout = image.layout,
        });

        writeDescriptors.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = set,
            .dstBinding = image.binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = image.types,
            .pImageInfo = &imageInfos.at(imageInfos.size() - 1),
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr,
        });
    }

    vkUpdateDescriptorSets(m_Device, writeDescriptors.size(), writeDescriptors.data(), 0, nullptr);

    if (m_DebugName.has_value()) {
        Debug::setDebugName(
            m_Device, VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)set, m_DebugName.value().c_str());
    }

    return set;
}
