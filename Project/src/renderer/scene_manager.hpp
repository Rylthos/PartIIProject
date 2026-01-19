#pragma once

#include "acceleration_structure_manager.hpp"
#include "events/events.hpp"

#include <filesystem>
#include <set>
#include <vector>

class SceneManager {
  public:
    static SceneManager* getManager()
    {
        static SceneManager manager;
        return &manager;
    }

    std::function<void(const Event&)> getUIEvent()
    {
        return std::bind(&SceneManager::UI, this, std::placeholders::_1);
    }

  private:
    SceneManager();

    void UI(const Event& event);

    void getDirectories();
    void getFileEntries();

    void handleDirectoryEntries(std::vector<uint8_t> data);
    void handleFileEntries(std::vector<uint8_t> data);

  private:
    std::filesystem::path m_CurrentPath;
    std::filesystem::path m_SelectedPath;

    bool m_ValidStructures[static_cast<uint8_t>(ASType::MAX_TYPE)];

    std::set<std::string> m_FileEntries;
    std::vector<std::filesystem::path> m_Directories;

    bool m_RequestedDirectories = false;
    bool m_RequestedEntries = false;
};
