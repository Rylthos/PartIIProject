#include "debug_utils.hpp"
#include <initializer_list>
#include <vulkan/vulkan_core.h>

static PFN_vkSetDebugUtilsObjectNameEXT pfnSetDebugUtilsObjectName;
static PFN_vkCmdBeginDebugUtilsLabelEXT pfnCmdBeginDebugUtilsLabel;
static PFN_vkCmdEndDebugUtilsLabelEXT pfnCmdEndDebugUtilsLabel;
static PFN_vkCmdInsertDebugUtilsLabelEXT pfnCmdInsertDebugUtilsLabel;

namespace Debug {
void setupDebugUtils(VkDevice device)
{
    pfnSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(
        device, "vkSetDebugUtilsObjectNameEXT");

    pfnCmdBeginDebugUtilsLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(
        device, "vkCmdBeginDebugUtilsLabelEXT");

    pfnCmdEndDebugUtilsLabel
        = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");

    pfnCmdInsertDebugUtilsLabel = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetDeviceProcAddr(
        device, "vkCmdInsertDebugUtilsLabelEXT");
}

void setDebugName(VkDevice device, VkObjectType objectType, uint64_t handle, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .pNext = nullptr,
        .objectType = objectType,
        .objectHandle = handle,
        .pObjectName = name,
    };
    VK_CHECK(pfnSetDebugUtilsObjectName(device, &nameInfo), "Failed to set object name");
}

void beginCmdDebugLabel(VkCommandBuffer cmd, const char* label, std::vector<float> colour)
{
    assert(colour.size() == 4 && "Must have 4 colours provided");

    VkDebugUtilsLabelEXT labelInfo {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label,
        .color = { colour[0], colour[1], colour[2], colour[3] },
    };
    pfnCmdBeginDebugUtilsLabel(cmd, &labelInfo);
}

void insertCmdDebugLabel(VkCommandBuffer cmd, const char* label, std::vector<float> colour)
{
    assert(colour.size() == 4 && "Must have 4 colours provided");

    VkDebugUtilsLabelEXT labelInfo {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pNext = nullptr,
        .pLabelName = label,
        .color = { colour[0], colour[1], colour[2], colour[3] },
    };
    pfnCmdInsertDebugUtilsLabel(cmd, &labelInfo);
}

void endCmdDebugLabel(VkCommandBuffer cmd) { pfnCmdEndDebugUtilsLabel(cmd); }
}
