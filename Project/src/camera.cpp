#include "camera.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>

#include "events.hpp"
#include "glm/geometric.hpp"
#include "glm/gtx/string_cast.hpp"

#include "imgui.h"

Camera::Camera(glm::vec3 origin, float pitch, float yaw)
    : m_Position(origin), m_Pitch(pitch), m_Yaw(yaw)
{
    updateVectors();
}

Camera::~Camera() { }

void Camera::keyboardEvent(const Event& event)
{
    const KeyboardEvent& keyEvent = static_cast<const KeyboardEvent&>(event);

    if (keyEvent.type() == KeyboardEventType::PRESS) {
        const KeyboardPressEvent& pressEvent = static_cast<const KeyboardPressEvent&>(keyEvent);

        m_PressedKeys.insert(pressEvent.keycode);
    }

    if (keyEvent.type() == KeyboardEventType::RELEASE) {
        const KeyboardReleaseEvent& releaseEvent
            = static_cast<const KeyboardReleaseEvent&>(keyEvent);

        m_PressedKeys.erase(releaseEvent.keycode);
    }
}

void Camera::mouseEvent(const Event& event)
{
    const MouseEvent& mouseEvent = static_cast<const MouseEvent&>(event);

    if (mouseEvent.type() == MouseEventType::MOVE) {
        const MouseMoveEvent& mmEvent = static_cast<const MouseMoveEvent&>(mouseEvent);
        m_Yaw += mmEvent.delta.x / 30.0f;
        m_Pitch -= mmEvent.delta.y / 30.0f;

        m_Pitch = std::clamp(m_Pitch, -89.9f, 89.9f);

        updateVectors();
    }
}

void Camera::frameEvent(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("Camera")) {
            ImGui::Text("Position: %.4f %.4f %.4f", m_Position.x, m_Position.y, m_Position.z);
            ImGui::Text("Pitch: %.4f", m_Pitch);
            ImGui::Text("Yaw: %.4f", m_Yaw);
            ImGui::Text("Forward: %.4f %.4f %.4f", m_Forward.x, m_Forward.y, m_Forward.z);
            ImGui::Text("Right  : %.4f %.4f %.4f", m_Right.x, m_Right.y, m_Right.z);
            ImGui::Text("Up     : %.4f %.4f %.4f", m_Up.x, m_Up.y, m_Up.z);
        }
        ImGui::End();
    }

    if (frameEvent.type() == FrameEventType::UPDATE) {
        const UpdateEvent& updateEvent = static_cast<const UpdateEvent&>(frameEvent);

        glm::vec3 resultantForce { 0.0f };

        float speedup = 1.f;

        // Delta
        if (m_PressedKeys.count(GLFW_KEY_W))
            resultantForce += m_Forward * glm::vec3(1.f, 0.f, 1.f);
        if (m_PressedKeys.count(GLFW_KEY_S))
            resultantForce -= m_Forward * glm::vec3(1.f, 0.f, 1.f);
        if (m_PressedKeys.count(GLFW_KEY_A))
            resultantForce -= m_Right;
        if (m_PressedKeys.count(GLFW_KEY_D))
            resultantForce += m_Right;
        if (m_PressedKeys.count(GLFW_KEY_SPACE))
            resultantForce += m_WorldUp;
        if (m_PressedKeys.count(GLFW_KEY_LEFT_CONTROL))
            resultantForce -= m_WorldUp;
        if (m_PressedKeys.count(GLFW_KEY_LEFT_SHIFT))
            speedup *= 3.f;

        m_Position += resultantForce * m_MovementSpeed * updateEvent.delta * speedup;
    }
}

void Camera::updateVectors()
{
    float yaw = glm::radians(m_Yaw);
    float pitch = glm::radians(m_Pitch);

    m_Forward.x = glm::sin(yaw) * glm::cos(pitch);
    m_Forward.y = -glm::sin(pitch);
    m_Forward.z = glm::cos(yaw) * glm::cos(pitch);

    m_Forward = glm::normalize(m_Forward);

    m_Right = glm::normalize(glm::cross(m_Forward, m_WorldUp));
    m_Up = glm::normalize(glm::cross(m_Right, m_Forward));
}
