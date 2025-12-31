#pragma once

#include "events/events.hpp"

#include "modification/mod_type.hpp"

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
    Modification::ShapeInfo getShape() { return { m_CurrentShape, m_CurrentAdditional }; }
    float getDelay() { return m_PlacementDelay; }

  private:
    ModificationManager() { }

    void UI(const Event& event);

  private:
    Modification::Shape m_CurrentShape = Modification::Shape::VOXEL;
    glm::vec4 m_CurrentAdditional;
    glm::vec3 m_SelectedColour = glm::vec3(1.f);

    float m_PlacementDelay = 0.1f;
};
