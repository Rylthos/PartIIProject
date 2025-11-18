#include "pipeline_layout.hpp"

#include <initializer_list>
#include <vulkan/vulkan_core.h>

#include "debug_utils.hpp"

PipelineLayoutGenerator PipelineLayoutGenerator::start(VkDevice device)
{
    PipelineLayoutGenerator generator(device);
    return generator;
}

PipelineLayoutGenerator& PipelineLayoutGenerator::addDescriptorLayout(VkDescriptorSetLayout layout)
{
    m_Descriptors.push_back(layout);
    return *this;
}

PipelineLayoutGenerator& PipelineLayoutGenerator::addDescriptorLayouts(
    std::initializer_list<VkDescriptorSetLayout> layouts)
{
    m_Descriptors.insert(m_Descriptors.end(), layouts.begin(), layouts.end());

    return *this;
}

PipelineLayoutGenerator& PipelineLayoutGenerator::addPushConstant(
    VkShaderStageFlags flags, uint32_t offset, uint32_t size)
{
    m_PushConstants.push_back(VkPushConstantRange {
        .stageFlags = flags,
        .offset = offset,
        .size = size,
    });

    return *this;
}

PipelineLayoutGenerator& PipelineLayoutGenerator::setDebugName(const char* name)
{
    m_DebugName = std::string(name);

    return *this;
}

VkPipelineLayout PipelineLayoutGenerator::build()
{
    VkPipelineLayoutCreateInfo layoutCI {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(m_Descriptors.size()),
        .pSetLayouts = m_Descriptors.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(m_PushConstants.size()),
        .pPushConstantRanges = m_PushConstants.data(),
    };

    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(m_Device, &layoutCI, nullptr, &layout),
        "Failed to create pipeline layout");

    if (m_DebugName.has_value()) {
        Debug::setDebugName(m_Device, VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)layout,
            m_DebugName.value().c_str());
    }

    return layout;
}
