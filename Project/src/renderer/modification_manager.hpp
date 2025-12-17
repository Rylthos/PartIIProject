#pragma once

#include "events/events.hpp"

enum class ModificationShape : int {
    VOXEL = 0,
    SPHERE = 1,
    CUBE = 2,
    CUBOID = 3,
    MAX_SHAPE,
};

enum class ModificationType : int {
    ERASE = 0,
    PLACE = 1,
    MAX_TYPE,
};

struct ModificationShapeInfo {
    ModificationShape shape;
    glm::vec4 additional;
};

class ModificationManager {
  public:
    static ModificationManager* getManager()
    {
        static ModificationManager manager;
        return &manager;
    }

    std::function<void(const Event& event)> getUIEvent()
    {
        return std::bind(&ModificationManager::UI, this, std::placeholders::_1);
    }

    glm::vec3 getSelectedColour() { return m_SelectedColour; }
    ModificationShapeInfo getShape()
    {
        return { .shape = m_CurrentShape, .additional = m_CurrentAdditional };
    }

  private:
    ModificationManager() { }

    void UI(const Event& event);

  private:
    ModificationShape m_CurrentShape = ModificationShape::VOXEL;
    glm::vec4 m_CurrentAdditional;
    glm::vec3 m_SelectedColour = glm::vec3(1.f);
};
