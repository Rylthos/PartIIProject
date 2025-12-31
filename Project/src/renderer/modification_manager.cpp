#include "modification_manager.hpp"

#include "events/events.hpp"

#include "imgui.h"

#include <map>

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
            const char* previewValue = Modification::shapeToString[m_CurrentShape];

            ImGui::Text("Placement delay");
            ImGui::PushItemWidth(-1.0f);
            ImGui::SliderFloat("##PlacementDelay", &m_PlacementDelay, 0.01f, 2.0f);
            ImGui::PopItemWidth();

            ImGui::Text("Current Shape");
            ImGui::PushItemWidth(-1.f);
            if (ImGui::BeginCombo("##CurrentShape", previewValue)) {
                for (uint8_t i = 0; i < static_cast<uint8_t>(Modification::Shape::MAX_SHAPE); i++) {
                    const bool isSelected = (currentlySelectedID == i);
                    Modification::Shape currentType = static_cast<Modification::Shape>(i);
                    if (ImGui::Selectable(Modification::shapeToString[currentType], isSelected)) {
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
            case Modification::Shape::VOXEL:
                break;
            case Modification::Shape::SPHERE: {
                int radius = int(m_CurrentAdditional.x);

                if (ImGui::SliderInt("Radius", &radius, 1, 25)) {
                    m_CurrentAdditional.x = radius;
                }
                break;
            }
            case Modification::Shape::CUBE: {
                int sideLength = int(m_CurrentAdditional.x);

                if (ImGui::SliderInt("Side length", &sideLength, 1, 25)) {
                    m_CurrentAdditional.x = sideLength;
                }
                break;
            }
            case Modification::Shape::CUBOID: {
                glm::ivec3 currentAddtional = glm::ivec3(m_CurrentAdditional);

                if (ImGui::SliderInt("Forward", &currentAddtional.x, 1, 25)) {
                    m_CurrentAdditional.x = currentAddtional.x;
                }
                if (ImGui::SliderInt("Up", &currentAddtional.y, 1, 25)) {
                    m_CurrentAdditional.y = currentAddtional.y;
                }
                if (ImGui::SliderInt("Sideways", &currentAddtional.z, 1, 25)) {
                    m_CurrentAdditional.z = currentAddtional.z;
                }
                break;
            }
            case Modification::Shape::MAX_SHAPE:
                assert(false);
            };
        }
        ImGui::End();
    }
}
