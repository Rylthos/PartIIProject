#pragma once

#include "acceleration_structure_manager.hpp"
#include "events/events.hpp"
#include "network/node.hpp"

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

    void init(Network::NetworkingInfo info);

    std::function<void(const Event&)> getUIEvent()
    {
        return std::bind(&SceneManager::UI, this, std::placeholders::_1);
    }

    std::function<bool(const std::vector<uint8_t>&, uint32_t)> getHandleRequestFileEntries();
    std::function<bool(const std::vector<uint8_t>&, uint32_t)> getHandleRequestDirEntries();

    std::function<bool(const std::vector<uint8_t>&, uint32_t)> getHandleReturnFileEntries();
    std::function<bool(const std::vector<uint8_t>&, uint32_t)> getHandleReturnDirEntries();

  private:
    SceneManager();

    void UI(const Event& event);

    bool handleRequestFileEntries(const std::vector<uint8_t>& data, uint32_t messageID);
    bool handleRequestDirEntries(const std::vector<uint8_t>& data, uint32_t messageID);

    bool handleReturnFileEntries(const std::vector<uint8_t>& data, uint32_t messageID);
    bool handleReturnDirEntries(const std::vector<uint8_t>& data, uint32_t messageID);

    void getDirectories();
    void getFileEntries();

  private:
    Network::NetworkingInfo m_NetInfo;

    std::filesystem::path m_CurrentPath;
    std::filesystem::path m_SelectedPath;

    bool m_ValidStructures[static_cast<uint8_t>(ASType::MAX_TYPE)];

    std::set<std::string> m_FileEntries;
    std::vector<std::filesystem::path> m_Directories;
};
