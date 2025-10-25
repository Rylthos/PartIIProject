#pragma once

#include "accelerationStructure.hpp"

#include "../buffer.hpp"

class OctreeAS : public IAccelerationStructure {
  public:
    OctreeAS();
    ~OctreeAS();

    void init(ASStructInfo info) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet,
        VkExtent2D imageSize) override;

    void updateShaders() override;

  private:
    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

  private:
    ASStructInfo m_Info;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;
};
