#include "acceleration_structure_manager.hpp"
#include "accelerationStructures/accelerationStructure.hpp"
#include "accelerationStructures/grid.hpp"

#include "logger.hpp"

#include <cassert>

void ASManager::init(ASStructInfo initInfo)
{
    m_InitInfo = initInfo;
    setAS(m_CurrentType);
}

void ASManager::cleanup() { delete m_CurrentAS.release(); }

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
        m_CurrentAS->init(m_InitInfo);
        break;
    default:
        assert(false && "Invalid Type provided");
    }
}
