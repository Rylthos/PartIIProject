#include "performance_logger.hpp"

#include "acceleration_structure_manager.hpp"
#include "events/events.hpp"
#include "imgui.h"

#include "logger/logger.hpp"

#include "shader_manager.hpp"

#include <memory>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <tracy/TracyC.h>

PerformanceLogger::PerformanceLogger()
{
    m_CurrentPath = std::filesystem::current_path();
    if (std::filesystem::exists(m_CurrentPath / "res" / "perf")) {
        m_CurrentPath /= "res";
        m_CurrentPath /= "perf";
    }

    getEntries();
}

void PerformanceLogger::init(Camera* camera) { m_Camera = camera; }

void PerformanceLogger::addGPUTime(float gpuTime)
{
    if (!m_Running)
        return;

    if (!(m_CurrentDelay < m_PerfEntries[m_CurrentEntry].delay)) {
        m_DataEntries[m_CurrentEntry].gpuFrameTimes.push_back(gpuTime);
    }
}

void PerformanceLogger::frameEvent(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        UI();
    } else if (frameEvent.type() == FrameEventType::UPDATE) {
        const UpdateEvent& updateEvent = static_cast<const UpdateEvent&>(frameEvent);

        update(updateEvent.delta);
    }
}

void PerformanceLogger::UI()
{
    if (ImGui::Begin("Performance log")) {
        {
            static int itemIndex = -1;
            ImGui::Text("Current file");
            if (ImGui::BeginListBox(
                    "##DirectoryEntries", { -1.0f, 6 * ImGui::GetTextLineHeightWithSpacing() })) {
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

                for (int j = m_Directories.size(); j < m_Directories.size() + m_FileEntries.size();
                    j++) {

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
            startLog(m_Selected);
        }

        ImGui::Text("Status: %s", m_Running ? "Running" : "Idle");

        if (m_Running) {
            const PerfEntry& entry = m_PerfEntries[m_CurrentEntry];
            ImGui::Text("Entry  : %s", entry.name.c_str());
            ImGui::Text("Delay  : %u/%d", m_CurrentDelay, entry.delay);
            ImGui::Text("Capture: %u/%d", m_CurrentCaptures, entry.captures);
        }
    }
    ImGui::End();
}

void PerformanceLogger::update(float delta)
{
    if (!m_Running)
        return;

    const PerfEntry& entry = m_PerfEntries[m_CurrentEntry];

    if (m_CurrentDelay < entry.delay) {
        m_CurrentDelay++;
        return;
    }

    m_CurrentCaptures++;

    if (m_CurrentCaptures > entry.captures) {
        Data& data = m_DataEntries[m_CurrentEntry];
        data.memoryUsage = ASManager::getManager()->getMemoryUsage();
        data.voxels = ASManager::getManager()->getVoxels();
        data.nodes = ASManager::getManager()->getNodes();

        m_CurrentEntry++;

        if (m_CurrentEntry < m_PerfEntries.size()) {
            startPerf(m_PerfEntries[m_CurrentEntry]);
        } else {
            savePerf();
        }
    }
}

void PerformanceLogger::startLog(std::filesystem::path file)
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

    m_PerfName = file.stem().string();
    parseJson(file);
}

void PerformanceLogger::parseJson(std::filesystem::path file)
{
    using json = nlohmann::json;

    std::ifstream input(file.string());

    if (!input.is_open()) {
        LOG_ERROR("Failed to open file {}", file.string());
        return;
    }

    m_Defaults = PerfEntry {};
    json data = json::parse(input);

    if (data.contains("defaults")) {
        json defaults = data["defaults"];
        m_Defaults = parseEntry(defaults);
    }

    if (data.contains("tests")) {
        json tests = data["tests"];
        for (int i = 0; i < tests.size(); i++) {
            PerfEntry entry = parseEntry(tests[i]);
            m_PerfEntries.push_back(entry);
        }
    }

    m_Running = true;
    m_CurrentEntry = 0;

    startPerf(m_PerfEntries[m_CurrentEntry]);
}

void PerformanceLogger::startPerf(const PerfEntry& perf)
{
    assert(m_Camera);

    m_CurrentCaptures = 0;
    m_CurrentDelay = 0;

    m_DataEntries.push_back({});
    m_DataEntries[m_CurrentEntry].gpuFrameTimes.reserve(perf.captures);

    m_Camera->setPosition(perf.camera_pos);
    m_Camera->setRotation(perf.yaw, perf.pitch);

    if (perf.structure != ASType::MAX_TYPE) {
        ASManager::getManager()->setAS(perf.structure);

        if (perf.scene != "") {
            bool validStructures[static_cast<uint8_t>(ASType::MAX_TYPE)];
            validStructures[static_cast<uint8_t>(perf.structure)] = true;
            ASManager::getManager()->loadAS(perf.scene, validStructures);
        }
    }

    ShaderManager::getInstance()->setMacro("STEP_LIMIT", std::format("{}", perf.steps));
    ShaderManager::getInstance()->updateShaders();

    LOG_INFO("Start Perf {}", perf.name);
}

PerformanceLogger::PerfEntry PerformanceLogger::parseEntry(const nlohmann::json& json)
{
    PerfEntry entry = m_Defaults;

    if (json.contains("name")) {
        entry.name = json["name"].get<std::string>();
    }

    if (json.contains("scene")) {
        entry.scene = json["scene"].get<std::string>();
    }

    if (json.contains("structure")) {
        std::string structure = json["structure"].get<std::string>();
        if (structure == "Grid") {
            entry.structure = ASType::GRID;
        } else if (structure == "Texture") {
            entry.structure = ASType::TEXTURE;
        } else if (structure == "Octree") {
            entry.structure = ASType::OCTREE;
        } else if (structure == "Contree") {
            entry.structure = ASType::CONTREE;
        } else if (structure == "Brickmap") {
            entry.structure = ASType::BRICKMAP;
        } else {
            LOG_ERROR("Unknown structure: {}", structure);
        }
    }

    if (json.contains("steps")) {
        entry.steps = json["steps"].get<int>();
    }

    if (json.contains("captures")) {
        entry.captures = json["captures"].get<int>();
    }

    if (json.contains("delay")) {
        entry.delay = json["delay"].get<int>();
    }

    if (json.contains("camera")) {
        auto cam = json["camera"];
        if (cam.contains("pos")) {
            if (cam["pos"].size() != 3) {
                LOG_ERROR("Invalid number of entries for camera position");
            } else {
                entry.camera_pos.x = cam["pos"][0].get<float>();
                entry.camera_pos.y = cam["pos"][1].get<float>();
                entry.camera_pos.z = cam["pos"][2].get<float>();
            }
        }

        if (cam.contains("rot")) {
            if (cam["rot"].size() != 2) {
                LOG_ERROR("Invalid number of entries for camera rotation");
            } else {
                entry.yaw = cam["rot"][0].get<float>();
                entry.pitch = cam["rot"][1].get<float>();
            }
        }
    }

    return entry;
}

void PerformanceLogger::savePerf()
{
    m_Running = false;

    using json = nlohmann::json;
    json output;

    std::vector<json> values;
    for (size_t i = 0; i < m_PerfEntries.size(); i++) {
        const PerfEntry& entry = m_PerfEntries[i];
        const Data& data = m_DataEntries[i];

        json value;

        value["name"] = entry.name;

        value["frametimes"] = data.gpuFrameTimes;

        value["stats"] = {
            { "memory", data.memoryUsage },
            { "voxels", data.voxels      },
            { "nodes",  data.nodes       },
        };

        values.push_back(value);
    }
    output["values"] = values;

    std::string filename = std::format("res/perf_output/{}.json", m_PerfName);
    std::ofstream outputStream(filename, std::ios::out);

    outputStream << output.dump(2) << std::endl;

    outputStream.close();

    LOG_INFO("Wrote perf file {}", filename);
}

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
