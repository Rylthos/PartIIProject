#include "performance_logger.hpp"

#include "imgui.h"
#include <filesystem>

#include "logger/logger.hpp"
#include "pgbar/details/prefabs/BasicConfig.hpp"

PerformanceLogger::PerformanceLogger()
{
    m_CurrentPath = std::filesystem::current_path();
    if (std::filesystem::exists(m_CurrentPath / "res" / "perf")) {
        m_CurrentPath /= "res";
        m_CurrentPath /= "perf";
    }

    getEntries();
}

void PerformanceLogger::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("Performance log")) {
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
                                getEntries();
                            } else {
                                itemIndex = i;
                            }
                        }

                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    for (int j = m_Directories.size();
                        j < m_Directories.size() + m_FileEntries.size(); j++) {

                        int i = j - m_Directories.size();

                        const bool isSelected = (itemIndex == j);

                        if (ImGui::Selectable(m_FileEntries[i].stem().c_str(), isSelected)) {
                            itemIndex = j;
                            m_Selected = m_FileEntries[i];
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
                getEntries();
            }

            ImGui::SameLine();

            if (ImGui::Button("Run Perf")) {
                startPerf(m_Selected);
            }
        }
        ImGui::End();
    }
}

void PerformanceLogger::startPerf(std::filesystem::path file)
{
    if (!std::filesystem::exists(file)) {
        LOG_ERROR("File '{}' does not exist", file.string());
        return;
    }

    if (!std::filesystem::is_regular_file(file)) {
        LOG_ERROR("Expected normal file: {}", file.string());
        return;
    }

    LOG_INFO("Running Perf: {}", file.filename().string());

    parseJson();
}

void PerformanceLogger::parseJson() { m_Defaults = Defaults {}; }

void PerformanceLogger::getEntries()
{
    m_Directories.clear();
    m_FileEntries.clear();

    for (auto const& entry : std::filesystem::directory_iterator { m_CurrentPath }) {
        if (entry.is_directory()) {
            m_Directories.push_back(entry.path());
        } else if (entry.is_regular_file()) {
            m_FileEntries.push_back(entry.path());
        }
    }
}
