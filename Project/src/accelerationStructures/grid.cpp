#include "grid.hpp"
#include <vulkan/vulkan_core.h>

#include "../logger.hpp"
#include "accelerationStructure.hpp"

GridAS::GridAS() { }

GridAS::~GridAS() { }

void GridAS::init(ASStructInfo info) { m_Info = info; }

void GridAS::render(VkCommandBuffer cmd, uint32_t currentFrame) { LOG_INFO("Render"); }
