#pragma once

#include "accelerationStructure.hpp"
#include <vulkan/vulkan_core.h>

class GridAS : public IAccelerationStructure {
  public:
    GridAS();
    ~GridAS();

    void init(ASStructInfo info) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet,
        VkExtent2D imageSize) override;

  private:
    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

  private:
    ASStructInfo m_Info;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;
};
