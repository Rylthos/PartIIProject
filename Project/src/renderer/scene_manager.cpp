#include "scene_manager.hpp"

#include "events/events.hpp"

#include "logger/logger.hpp"

#include "acceleration_structure_manager.hpp"

#include <imgui.h>

SceneManager::SceneManager()
{

    m_CurrentPath = std::filesystem::current_path();
    if (std::filesystem::exists(m_CurrentPath / "res" / "structures")) {
        m_CurrentPath /= "res";
        m_CurrentPath /= "structures";
    }
    getDirectories();
}

void SceneManager::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("Scene manager")) {
            {
                static int itemIndex = -1;
                ImGui::Text("Current file");
                if (ImGui::BeginListBox("##DirectoryEntries",
                        { -1.0f, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
                    for (int i = 0; i < m_Directories.size(); i++) {
                        const bool isSelected = (itemIndex == i);
                        if (ImGui::Selectable(m_Directories[i].stem().c_str(), isSelected)) {
                            if (itemIndex == i) {
                                itemIndex = -1;
                                m_CurrentPath = m_Directories[i];
                                getDirectories();
                            } else {
                                itemIndex = i;
                                m_SelectedPath = m_Directories[i];
                                getFileEntries();
                            }
                        }

                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndListBox();
                }
            }

            if (ImGui::Button("Back")) {
                m_CurrentPath = m_CurrentPath.parent_path();
                getDirectories();
            }

            ImGui::SameLine();

            if (ImGui::Button("Load structure")) {
                ASManager::getManager()->loadAS(m_SelectedPath, m_ValidStructures);
            }

            {
                auto addText = [&](const char* extension, const char* text, ASType type) {
                    ImVec4 colour;
                    bool valid = m_FileEntries.contains(extension);
                    m_ValidStructures[static_cast<uint8_t>(type)] = valid;
                    colour = valid ? ImVec4 { 0.0f, 1.0f, 0.0f, 1.0f }
                                   : ImVec4 { 1.0f, 0.0f, 0.0f, 1.0f };
                    ImGui::TextColored(colour, "%s", text);
                };

                addText(".voxgrid", "Grid", ASType::GRID);
                addText(".voxtexture", "Texture", ASType::TEXTURE);
                addText(".voxoctree", "Octree", ASType::OCTREE);
                addText(".voxcontree", "Contree", ASType::CONTREE);
                addText(".voxbrick", "Brickmap", ASType::BRICKMAP);
            }
        }
        ImGui::End();
    }
}

void SceneManager::getDirectories()
{
    m_Directories.clear();

    for (auto const& entry : std::filesystem::directory_iterator { m_CurrentPath }) {
        if (entry.is_directory()) {
            m_Directories.push_back(entry.path());
        }
    }
}

void SceneManager::getFileEntries()
{
    std::filesystem::path folderName = m_SelectedPath.filename();

    m_FileEntries.clear();

    for (auto const& entry : std::filesystem::directory_iterator { m_SelectedPath }) {
        if (entry.is_regular_file()) {
            if (entry.path().stem() == folderName) {
                m_FileEntries.insert(entry.path().extension());
            }
        }
    }
}
