#pragma once

#include "../image.hpp"
#include "acceleration_structure.hpp"
#include <algorithm>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

class TextureAS : public IAccelerationStructure {
    using GridVoxel = glm::u8vec4;

  public:
    TextureAS();
    ~TextureAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override
    {
        return m_Dimensions.x * m_Dimensions.y * m_Dimensions.z * 4 * sizeof(uint8_t);
    }

  private:
    void createDescriptorLayouts();
    void destroyDescriptorLayouts();

    void createImages();
    void destroyImages();

    void createDescriptorSets();
    void freeDescriptorSets();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();

    void createRenderPipeline();
    void destroyRenderPipeline();

  private:
    std::vector<GridVoxel> m_Voxels;
    glm::uvec3 m_Dimensions;

    VkDescriptorSetLayout m_ImageSetLayout;
    VkDescriptorSet m_ImageSet = VK_NULL_HANDLE;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;

    Image m_DataImage;

    bool m_UpdateBuffers = false;
};
