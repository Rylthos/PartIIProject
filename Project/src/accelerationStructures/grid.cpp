#include "grid.hpp"
#include <vulkan/vulkan_core.h>

#include "../logger.hpp"

GridAS::GridAS() { }

GridAS::~GridAS() { }

void GridAS::init(VkDevice device, VmaAllocator allocator) { }

void GridAS::render(VkCommandBuffer cmd, uint32_t currentFrame) { LOG_INFO("Render"); }
