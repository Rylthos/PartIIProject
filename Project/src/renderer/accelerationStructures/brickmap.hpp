#pragma once

#include "acceleration_structure.hpp"
#include <stop_token>
#include <vulkan/vulkan_core.h>

#include "../buffer.hpp"
#include "generators/brickmap.hpp"

#include <vector>

class BrickmapAS : public IAccelerationStructure {

  public:
    BrickmapAS();
    ~BrickmapAS();

    void init(ASStructInfo info) override;
    void fromLoader(std::unique_ptr<Loader>&& loader) override;
    void fromFile(std::filesystem::path path) override;
    void render(VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet,
        VkExtent2D imageSize) override;
    void update(float dt) override;

    void updateShaders() override;

    uint64_t getMemoryUsage() override
    {
        return m_BrickgridBuffer.getSize() + m_BrickmapsBuffer.getSize() + m_ColourBuffer.getSize();
    }

    glm::uvec3 getDimensions() override { return m_BrickgridSize * 8u; }

  private:
    void mainRender(
        VkCommandBuffer cmd, Camera camera, VkDescriptorSet renderSet, VkExtent2D imageSize);
    void modRender(VkCommandBuffer cmd, Camera& camera);
    void requestRender(VkCommandBuffer cmd);

    void createDescriptorLayout();
    void destroyDescriptorLayout();

    void createBuffers();

    void createBrickgridBuffers();
    void createHelperBuffers();

    void freeBuffers();

    void resizeFree(VkCommandBuffer cmd);
    void resizeBricks(VkCommandBuffer cmd);
    void resizeColour(VkCommandBuffer cmd);
    void resizeFreeColour(VkCommandBuffer cmd);

    void createDescriptorSet();
    void freeDescriptorSet();

    void createRenderPipelineLayout();
    void destroyRenderPipelineLayout();
    void createRenderPipeline();
    void destroyRenderPipeline();

    void createModPipelineLayout();
    void destroyModPipelineLayout();
    void createModPipeline();
    void destroyModPipeline();

    void createRequestPipelineLayout();
    void destroyRequestPipelineLayout();
    void createRequestPipeline();
    void destroyRequestPipeline();

  private:
    VkDescriptorSetLayout m_BufferSetLayout;
    VkDescriptorSet m_BufferSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_ModSetLayout;
    VkDescriptorSet m_ModSet = VK_NULL_HANDLE;

    VkPipelineLayout m_RenderPipelineLayout;
    VkPipeline m_RenderPipeline;

    VkPipelineLayout m_ModPipelineLayout;
    VkPipeline m_ModPipeline;

    VkPipelineLayout m_RequestPipelineLayout;
    VkPipeline m_RequestPipeline;

    Buffer m_BrickgridBuffer;
    Buffer m_BrickmapsBuffer;
    Buffer m_ColourBuffer;

    uint32_t m_BrickmapCount;
    uint32_t m_FreeBrickCount;
    uint32_t m_ColourBlockCount;
    uint32_t m_FreeColourCount;

    const uint32_t m_ColourBlockIncrease = 100;

    uint32_t m_Requests = 1024;
    Buffer m_RequestBuffer;
    uint32_t* m_MappedRequestBuffer = nullptr;

    Buffer m_FreeBricks;
    uint32_t* m_MappedFreeBricks = nullptr;

    Buffer m_FreeColours;
    uint32_t* m_MappedFreeColours = nullptr;

    glm::uvec3 m_BrickgridSize;

    std::vector<Generators::BrickgridPtr> m_Brickgrid;
    std::vector<Generators::Brickmap> m_Brickmaps;
    std::vector<Generators::BrickmapColour> m_Colours;

    bool m_UpdateBuffers = false;

    bool m_DoubleFreeBricks = false;
    bool m_DoubleFreeColours = false;
    bool m_DoubleBricks = false;
    bool m_IncreaseColour = false;

    Buffer m_TempBuffer;

    bool m_ReallocFreeBricks = false;
    bool m_ReallocFreeColours = false;
    bool m_ReallocBricks = false;
    bool m_ReallocColour = false;
};
