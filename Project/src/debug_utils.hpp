#pragma once

#include <vulkan/vulkan.h>

#include "logger.hpp"

void setupDebugUtils(VkDevice device);

void setDebugName(VkDevice device, VkObjectType objectType, uint64_t handle, const char* name);

void beginCmdDebugLabel(VkCommandBuffer cmd, const char* label, std::vector<float> colour);
void insertCmdDebugLabel(VkCommandBuffer cmd, const char* label, std::vector<float> colour);
void endCmdDebugLabel(VkCommandBuffer cmd);
