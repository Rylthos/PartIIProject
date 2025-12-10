#include "modification_manager.hpp"

#include "events/events.hpp"

#include "imgui.h"

#include <map>

static std::map<ModificationShape, const char*> shapeToString {
    { ModificationShape::VOXEL,  "Single Voxel" },
    { ModificationShape::SPHERE, "Sphere"       },
};

void ModificationManager::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("Modification")) {
            // Colour Select
            ImGui::Text("Voxel colour");
            ImGui::ColorEdit3(
                "##VoxelColour", (float*)&m_SelectedColour[0], ImGuiColorEditFlags_DisplayHSV);

            int currentlySelectedID = static_cast<int>(m_CurrentShape);
            const char* previewValue = shapeToString[m_CurrentShape];

            ImGui::Text("Current Shape");
            ImGui::PushItemWidth(-1.f);
            if (ImGui::BeginCombo("##CurrentShape", previewValue)) {
                for (uint8_t i = 0; i < static_cast<uint8_t>(ModificationShape::MAX_SHAPE); i++) {
                    const bool isSelected = (currentlySelectedID == i);
                    ModificationShape currentType = static_cast<ModificationShape>(i);
                    if (ImGui::Selectable(shapeToString[currentType], isSelected)) {
                        m_CurrentShape = currentType;
                        m_CurrentAdditional = glm::vec4(1.f);
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            switch (m_CurrentShape) {
            case ModificationShape::VOXEL:
                break;
            case ModificationShape::SPHERE: {
                int radius = int(m_CurrentAdditional.x);

                if (ImGui::SliderInt("Radius", &radius, 1, 25)) {
                    m_CurrentAdditional.x = radius;
                }
                break;
            }
            case ModificationShape::MAX_SHAPE:
                assert(false);
            };
        }
        ImGui::End();
    }
}
