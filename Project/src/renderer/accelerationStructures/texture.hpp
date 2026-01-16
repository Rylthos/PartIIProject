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
    void fromRaw(std::vector<uint8_t> rawData, bool shouldReset) override;
    void fromFile(std::filesystem::path path) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override
    {
        return m_Dimensions.x * m_Dimensions.y * m_Dimensions.z * 4 * sizeof(uint8_t);
    }

    glm::uvec3 getDimensions() override { return m_Dimensions; }

    bool canAnimate() override { return true; }

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

    void createModPipelineLayout();
    void destroyModPipelineLayout();

    void createModPipeline();
    void destroyModPipeline();

  private:
    std::vector<Generators::TextureVoxel> m_Voxels;
    glm::uvec3 m_Dimensions;

    VkDescriptorSetLayout m_ImageSetLayout;
    VkDescriptorSet m_ImageSet = VK_NULL_HANDLE;

    VkPipeline m_RenderPipeline;
    VkPipelineLayout m_RenderPipelineLayout;

    VkPipeline m_ModPipeline;
    VkPipelineLayout m_ModPipelineLayout;

    Image m_DataImage;

    bool m_UpdateBuffers = false;
};
