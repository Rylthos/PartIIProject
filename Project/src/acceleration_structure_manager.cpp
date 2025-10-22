#include "acceleration_structure_manager.hpp"
#include "accelerationStructures/accelerationStructure.hpp"
#include "accelerationStructures/grid.hpp"

#include <cassert>

void ASManager::init(ASManagerStructureInfo initInfo)
{
    m_InitInfo = initInfo;
    setAS(m_CurrentType);
}

void ASManager::cleanup() { m_CurrentAS.reset(); }

void ASManager::render(
    VkCommandBuffer cmd, Camera camera, VkDescriptorSet drawImageSet, VkExtent2D imageSize)
{
    assert(m_CurrentAS);

    m_CurrentAS->render(cmd, camera, drawImageSet, imageSize);
}

void ASManager::setAS(ASType type)
{
    switch (type) {
    case ASType::GRID:
        m_CurrentAS = std::make_unique<GridAS>();
        m_CurrentAS->init({
            .device = m_InitInfo.device,
            .allocator = m_InitInfo.allocator,
            .graphicsQueue = m_InitInfo.graphicsQueue,
            .graphicsQueueIndex = m_InitInfo.graphicsQueueIndex,
            .descriptorPool = m_InitInfo.descriptorPool,
            .drawImageDescriptorLayout = m_InitInfo.drawImageDescriptorLayout,
        });
        break;
    default:
        assert(false && "Invalid Type provided");
    }
}
