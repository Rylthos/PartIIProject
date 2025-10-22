#pragma once

#include "accelerationStructure.hpp"
#include <vulkan/vulkan_core.h>

class GridAS : public IAccelerationStructure {
  public:
    GridAS();
    ~GridAS();

    void init(ASStructInfo info) override;
    void render(VkCommandBuffer cmd, uint32_t currentFrame) override;

  private:
    ASStructInfo m_Info;
};
