#pragma once

#include "logger/logger.hpp"

#include <vulkan/vulkan.h>

namespace Debug {
void setupDebugUtils(VkDevice device);

void setDebugName(VkDevice device, VkObjectType objectType, uint64_t handle, const char* name);

void beginCmdDebugLabel(VkCommandBuffer cmd, const char* label, std::vector<float> colour);
void insertCmdDebugLabel(VkCommandBuffer cmd, const char* label, std::vector<float> colour);
void endCmdDebugLabel(VkCommandBuffer cmd);
}
