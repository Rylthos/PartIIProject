#pragma once

#include "../image.hpp"
#include "acceleration_structure.hpp"
#include <vulkan/vulkan_core.h>

#include "generators/texture.hpp"

class TextureAS : public IAccelerationStructure {
  public:
    TextureAS();
    ~TextureAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void fromFile(std::filesystem::path path) override;
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
    std::vector<Generators::TextureVoxel> m_Voxels;
    glm::uvec3 m_Dimensions;

    VkDescriptorSetLayout m_ImageSetLayout;
    VkDescriptorSet m_ImageSet = VK_NULL_HANDLE;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;

    Image m_DataImage;

    bool m_UpdateBuffers = false;
};
