#pragma once

#include <set>

#include "events/event_dispatcher.hpp"
#include "events/events.hpp"

#include "glm/glm.hpp"

class Camera : public EventDispatcher {
  public:
    Camera(glm::vec3 origin = glm::vec3(0.0f, 0.0f, 0.0f), float pitch = 0.0f, float yaw = 0.0f);
    ~Camera();

    std::function<void(const Event& event)> getKeyboardEvent()
    {
        return std::bind(&Camera::keyboardEvent, this, std::placeholders::_1);
    }

    std::function<void(const Event& event)> getMouseEvent()
    {
        return std::bind(&Camera::mouseEvent, this, std::placeholders::_1);
    }

    std::function<void(const Event& event)> getFrameEvent()
    {
        return std::bind(&Camera::frameEvent, this, std::placeholders::_1);
    }

    void setPosition(glm::vec3 pos)
    {
        m_Position = pos;
        updateVectors();
        positionUpdated();
    }

    void setRotation(float yaw, float pitch)
    {
        m_Yaw = yaw;
        m_Pitch = pitch;
        updateVectors();
        rotationUpdated();
    }

    glm::vec3 getPosition() { return m_Position; }
    glm::vec3 getForwardVector() { return m_Forward; }
    glm::vec3 getRightVector() { return m_Right; }
    glm::vec3 getUpVector() { return m_Up; }

  private:
    glm::vec3 m_Position;
    float m_Pitch;
    float m_Yaw;

    const glm::vec3 m_WorldUp { 0.0, -1.0, 0.0 };
    const glm::vec3 m_WorldRight { 1.0, 0.0, 0.0 };
    const glm::vec3 m_WorldForward { 0.0, 0.0, 1.0 };

    glm::vec3 m_Forward;
    glm::vec3 m_Right;
    glm::vec3 m_Up;

    float m_MovementSpeed = 2.0f;
    std::set<uint32_t> m_PressedKeys;

  private:
    void keyboardEvent(const Event& event);
    void mouseEvent(const Event& event);
    void frameEvent(const Event& event);

    void positionUpdated();
    void rotationUpdated();

    void updateVectors();
};
