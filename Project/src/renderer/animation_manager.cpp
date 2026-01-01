#include "animation_manager.hpp"

#include "acceleration_structure_manager.hpp"

#include "events/events.hpp"
#include "imgui.h"

#include "imgui_internal.h"

void AnimationManager::reset()
{
    m_Playing = false;
    m_Paused = false;
    ASManager::getManager()->setAnimationFrame(m_CachedFrame);
}

void AnimationManager::frameEvent(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        UI();
    } else if (frameEvent.type() == FrameEventType::UPDATE) {
        const UpdateEvent& updateEvent = static_cast<const UpdateEvent&>(event);
        update(updateEvent.delta);
    }
}

void AnimationManager::UI()
{
    if (ImGui::Begin("Animation")) {
        const size_t frameCount = ASManager::getManager()->getAnimationFrames();
        ImGui::Text("Frame count  : %ld", frameCount);

        ImGui::Text("Current Frame");
        int32_t currentFrame = ASManager::getManager()->getAnimationFrame();
        const int32_t add_one = 1;
        if (ImGui::InputScalar("##CurrentAnimationFrame", ImGuiDataType_S32, &currentFrame,
                &add_one, NULL, "%d", 0)) {
            currentFrame = ((currentFrame % frameCount) + frameCount) % frameCount;
            ASManager::getManager()->setAnimationFrame(currentFrame);
        }

        ImGui::Text("FPS");
        ImGui::SameLine();
        ImGui::DragInt("##AnimationFPS", (int*)&m_FPS, 0.1f, 1);

        bool pop = false;
        if (m_Playing && !m_Paused) {
            imguiDisable();
            pop = true;
        }

        if (ImGui::Button("Play")) {
            if (!m_Playing) {
                m_CachedFrame = ASManager::getManager()->getAnimationFrame();
                m_Playing = true;
            }
            m_Paused = false;
        }

        if (pop) {
            imguiEnable();
        }

        pop = false;
        if (!m_Playing || m_Paused) {
            imguiDisable();
            pop = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            m_Paused = true;
        }

        if (pop) {
            imguiEnable();
        }

        pop = false;
        if (!m_Playing) {
            imguiDisable();
            pop = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            m_Playing = false;
        }

        if (pop) {
            imguiEnable();
        }
    }
    ImGui::End();
}

void AnimationManager::update(float delta)
{
    static float time = 0.0f;
    if (m_Playing && !m_Paused) {
        time += delta;

        if (time > (1.f / m_FPS)) {
            time = 0.0f;
            uint32_t nextFrame = (ASManager::getManager()->getAnimationFrame() + 1)
                % ASManager::getManager()->getAnimationFrames();
            ASManager::getManager()->setAnimationFrame(nextFrame);
        }
    }
}

void AnimationManager::imguiDisable()
{
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
}

void AnimationManager::imguiEnable()
{
    ImGui::PopItemFlag();
    ImGui::PopStyleVar();
}
