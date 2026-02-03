#include "scene_manager.hpp"

#include "events/events.hpp"

#include "logger/logger.hpp"

#include "network/loop.hpp"
#include "network/node.hpp"
#include "network_proto/header.pb.h"
#include "network_proto/messages.pb.h"

#include "acceleration_structure_manager.hpp"

#include <imgui.h>

#include <functional>

SceneManager::SceneManager() { }

void SceneManager::init(Network::NetworkingInfo info)
{
    m_NetInfo = info;

    m_CurrentPath = std::filesystem::current_path();
    if (std::filesystem::exists(m_CurrentPath / "res" / "structures")) {
        m_CurrentPath /= "res";
        m_CurrentPath /= "structures";
    }
    getDirectories();
}

std::function<bool(const std::vector<uint8_t>&, uint32_t)>
SceneManager::getHandleRequestFileEntries()
{
    using namespace std::placeholders;
    return std::bind(&SceneManager::handleRequestFileEntries, this, _1, _2);
}

std::function<bool(const std::vector<uint8_t>&, uint32_t)>
SceneManager::getHandleRequestDirEntries()
{
    using namespace std::placeholders;
    return std::bind(&SceneManager::handleRequestDirEntries, this, _1, _2);
}

std::function<bool(const std::vector<uint8_t>&, uint32_t)>
SceneManager::getHandleReturnFileEntries()
{
    using namespace std::placeholders;
    return std::bind(&SceneManager::handleReturnFileEntries, this, _1, _2);
}

std::function<bool(const std::vector<uint8_t>&, uint32_t)> SceneManager::getHandleReturnDirEntries()
{
    using namespace std::placeholders;
    return std::bind(&SceneManager::handleReturnDirEntries, this, _1, _2);
}

std::function<bool(const std::vector<uint8_t>&, uint32_t)> SceneManager::getHandleLoadScene()
{
    using namespace std::placeholders;
    return std::bind(&SceneManager::handleLoadScene, this, _1, _2);
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
                if (m_NetInfo.enableClientSide) {
                    LOG_INFO("Request Scene: {}", m_SelectedPath.string());
                    NetProto::LoadScene scene;
                    scene.set_scene(m_SelectedPath.string());
                    Network::sendMessage(NetProto::HEADER_TYPE_LOAD_SCENE, scene);
                } else {
                    ASManager::getManager()->loadAS(m_SelectedPath, m_ValidStructures);
                }
            }

            {
                updateValidStructures();
                auto addText = [&](const char* text, ASType type) {
                    ImVec4 colour;
                    bool valid = m_ValidStructures[static_cast<uint8_t>(type)];
                    colour = valid ? ImVec4 { 0.0f, 1.0f, 0.0f, 1.0f }
                                   : ImVec4 { 1.0f, 0.0f, 0.0f, 1.0f };
                    ImGui::TextColored(colour, "%s", text);
                };

                addText("Grid", ASType::GRID);
                addText("Texture", ASType::TEXTURE);
                addText("Octree", ASType::OCTREE);
                addText("Contree", ASType::CONTREE);
                addText("Brickmap", ASType::BRICKMAP);
            }
        }
        ImGui::End();
    }
}

bool SceneManager::handleRequestFileEntries(const std::vector<uint8_t>& data, uint32_t messageID)
{
    NetProto::RequestFileEntries entries;
    entries.ParseFromArray(data.data(), data.size());

    m_SelectedPath = entries.file();

    getFileEntries();

    LOG_INFO("Returning File entries");
    NetProto::ReturnFileEntries files;
    for (const auto& fileExt : m_FileEntries) {
        files.mutable_file_extensions()->Add(std::string(fileExt));
    }
    Network::sendMessage(NetProto::HEADER_TYPE_RETURN_FILE_ENTRIES, files);

    return true;
}

bool SceneManager::handleRequestDirEntries(const std::vector<uint8_t>& data, uint32_t messageID)
{
    LOG_INFO("Returning Dir entries");
    NetProto::ReturnDirEntries dirs;
    for (uint32_t i = 0; i < m_Directories.size(); i++) {
        dirs.mutable_dir_files()->Add(m_Directories[i]);
    }
    Network::sendMessage(NetProto::HEADER_TYPE_RETURN_DIR_ENTRIES, dirs);

    return true;
}

bool SceneManager::handleReturnFileEntries(const std::vector<uint8_t>& data, uint32_t messageID)
{
    LOG_INFO("Received File Entries");
    NetProto::ReturnFileEntries fileEntries;
    fileEntries.ParseFromArray(data.data(), data.size());

    m_FileEntries.clear();
    for (uint32_t i = 0; i < fileEntries.file_extensions_size(); i++) {
        m_FileEntries.insert(fileEntries.file_extensions().at(i));
    }

    return true;
}

bool SceneManager::handleReturnDirEntries(const std::vector<uint8_t>& data, uint32_t messageID)
{
    LOG_INFO("Received Dir Entries");
    NetProto::ReturnDirEntries dirEntries;
    dirEntries.ParseFromArray(data.data(), data.size());

    m_Directories.clear();
    for (uint32_t i = 0; i < dirEntries.dir_files_size(); i++) {
        m_Directories.push_back(dirEntries.dir_files().at(i));
    }
    std::sort(m_Directories.begin(), m_Directories.end());

    return true;
}

bool SceneManager::handleLoadScene(const std::vector<uint8_t>& data, uint32_t messageID)
{
    NetProto::LoadScene scene;
    scene.ParseFromArray(data.data(), data.size());

    LOG_INFO("REQUEST SCENE: {}", scene.scene());

    updateValidStructures();

    ASManager::getManager()->loadAS(scene.scene(), m_ValidStructures);

    return true;
}

void SceneManager::updateValidStructures()
{
    std::vector<std::pair<const char*, ASType>> points = {
        { ".voxgrid",    ASType::GRID     },
        { ".voxtexture", ASType::TEXTURE  },
        { ".voxoctree",  ASType::OCTREE   },
        { ".voxcontree", ASType::CONTREE  },
        { ".voxbrick",   ASType::BRICKMAP },
    };

    for (const auto& pair : points) {
        bool valid = m_FileEntries.contains(pair.first);
        m_ValidStructures[static_cast<uint8_t>(pair.second)] = valid;
    }
}

void SceneManager::getDirectories()
{
    if (m_NetInfo.enableClientSide) {
        LOG_INFO("REQUEST DIRECTORIES");
        std::vector<uint8_t> data;
        Network::sendMessage(NetProto::HEADER_TYPE_REQUEST_DIR_ENTRIES, data);
    } else {
        m_Directories.clear();

        for (auto const& entry : std::filesystem::directory_iterator { m_CurrentPath }) {
            if (entry.is_directory()) {
                m_Directories.push_back(entry.path());
            }
        }

        std::sort(m_Directories.begin(), m_Directories.end());
    }
}

void SceneManager::getFileEntries()
{
    if (m_NetInfo.enableClientSide) {
        LOG_INFO("REQUEST FILES");

        NetProto::RequestFileEntries fileEntries;
        fileEntries.set_file(m_SelectedPath);
        Network::sendMessage(NetProto::HEADER_TYPE_REQUEST_FILE_ENTRIES, fileEntries);
    } else {
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
}
