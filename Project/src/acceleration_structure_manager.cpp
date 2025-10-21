#include "acceleration_structure_manager.hpp"
#include "accelerationStructures/accelerationStructure.hpp"
#include "accelerationStructures/grid.hpp"

#include <cassert>

void ASManager::init(VkDevice device, VmaAllocator allocator)
{
    m_VkDevice = device;
    m_VmaAllocator = allocator;
}

void ASManager::cleanup() { m_CurrentAS.reset(); }

void ASManager::render(VkCommandBuffer cmd, uint32_t currentFrame)
{
    assert(m_CurrentAS);

    m_CurrentAS->render(cmd, currentFrame);
}

void ASManager::setAS(ASType type)
{
    switch (type) {
    case ASType::GRID:
        m_CurrentAS = std::make_unique<GridAS>();
        m_CurrentAS->init(m_VkDevice, m_VmaAllocator);
        break;
    default:
        assert(false && "Invalid Type provided");
    }
}
