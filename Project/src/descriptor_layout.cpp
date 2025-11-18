#include "descriptor_layout.hpp"
#include "debug_utils.hpp"
#include <vulkan/vulkan_core.h>

DescriptorLayoutGenerator DescriptorLayoutGenerator::start(VkDevice device)
{
    return DescriptorLayoutGenerator(device);
}

DescriptorLayoutGenerator& DescriptorLayoutGenerator::addBinding(
    VkDescriptorType type, VkShaderStageFlags flags, uint32_t binding, uint32_t descriptorCount)
{
    m_Bindings.push_back(VkDescriptorSetLayoutBinding {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = descriptorCount,
        .stageFlags = flags,
        .pImmutableSamplers = nullptr,
    });

    return *this;
}

DescriptorLayoutGenerator& DescriptorLayoutGenerator::addStorageBufferBinding(
    VkShaderStageFlags flags, uint32_t binding)
{
    return addBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, flags, binding, 1);
}

DescriptorLayoutGenerator& DescriptorLayoutGenerator::setDebugName(const char* name)
{
    m_DebugName = std::string(name);
    return *this;
}

VkDescriptorSetLayout DescriptorLayoutGenerator::build()
{
    VkDescriptorSetLayoutCreateInfo setLayoutCI {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(m_Bindings.size()),
        .pBindings = m_Bindings.data(),
    };

    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(m_Device, &setLayoutCI, nullptr, &layout);

    if (m_DebugName.has_value()) {
        Debug::setDebugName(m_Device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)layout,
            m_DebugName.value().c_str());
    }

    return layout;
}
