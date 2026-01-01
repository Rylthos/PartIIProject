#include "animation_manager.hpp"

#include "acceleration_structure_manager.hpp"

#include "imgui.h"

void AnimationManager::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("Animation")) {
            ImGui::Text("Frame count  : %ld", ASManager::getManager()->getAnimationFrames());

            ImGui::Text("Current Frame");
            uint32_t currentFrame = ASManager::getManager()->getAnimationFrame();
            const uint32_t add_one = 1;
            if (ImGui::InputScalar("##CurrentAnimationFrame", ImGuiDataType_U32, &currentFrame,
                    &add_one, NULL, "%d", 0)) {
                currentFrame = currentFrame % ASManager::getManager()->getAnimationFrames();
                ASManager::getManager()->setAnimationFrame(currentFrame);
            }
        }
        ImGui::End();
    }
}
