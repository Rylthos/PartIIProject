#include "scene_manager.hpp"

#include "events/events.hpp"

#include "logger/logger.hpp"

#include "acceleration_structure_manager.hpp"

#include <imgui.h>

#ifdef SERVER_CLIENT
#include "network/handlers/client.hpp"
#include <functional>
#endif

SceneManager::SceneManager()
{
#ifdef SERVER_CLIENT
#else
    m_CurrentPath = std::filesystem::current_path();
    if (std::filesystem::exists(m_CurrentPath / "res" / "structures")) {
        m_CurrentPath /= "res";
        m_CurrentPath /= "structures";
    }
    getDirectories();
#endif
}

void SceneManager::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
#ifdef SERVER_CLIENT
        if (!m_Requested) {
            requestEntries("");
            m_Requested = true;
        }
#endif
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
#ifdef SERVER_CLIENT
#else
                                getDirectories();
#endif
                            } else {
                                itemIndex = i;
                                m_SelectedPath = m_Directories[i];
#ifdef SERVER_CLIENT
#else
                                getFileEntries();
#endif
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
#ifdef SERVER_CLIENT
#else
                ASManager::getManager()->loadAS(m_SelectedPath, m_ValidStructures);
#endif
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
#ifdef SERVER_CLIENT
#else
    m_Directories.clear();

    for (auto const& entry : std::filesystem::directory_iterator { m_CurrentPath }) {
        if (entry.is_directory()) {
            m_Directories.push_back(entry.path());
        }
    }

    std::sort(m_Directories.begin(), m_Directories.end());
#endif
}

void SceneManager::getFileEntries()
{
#ifdef SERVER_CLIENT
#else
    std::filesystem::path folderName = m_SelectedPath.filename();

    m_FileEntries.clear();

    for (auto const& entry : std::filesystem::directory_iterator { m_SelectedPath }) {
        if (entry.is_regular_file()) {
            if (entry.path().stem() == folderName) {
                m_FileEntries.insert(entry.path().extension());
            }
        }
    }
#endif
}

#ifdef SERVER_CLIENT
void SceneManager::requestEntries(std::string path)
{
    Network::Client::addFileRequest(
        path, std::bind(&SceneManager::handleEntries, this, std::placeholders::_1));
    LOG_INFO("Requested Entries");
}

void SceneManager::handleEntries(std::optional<std::vector<uint8_t>> data)
{
    if (!data.has_value()) {
        LOG_ERROR("Failed to get data");
    }
    LOG_INFO("Got data: {}", data->size());
}
#endif
