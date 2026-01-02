#pragma once

#include "events/events.hpp"

#include "acceleration_structure_manager.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include <nlohmann/json.hpp>

class PerformanceLogger {

    struct CameraSettings {
        glm::vec3 pos = glm::vec3(0.f);
        float pitch = 0.0f;
        float yaw = 0.0f;
    };

    struct PerfEntry {
        std::string name = "";
        std::string id = "";
        std::string scene = "";
        ASType structure = ASType::MAX_TYPE;

        uint32_t steps = 100;
        uint32_t captures = 10;
        uint32_t delay = 10;
        CameraSettings camera;
    };

    struct Data {
        std::vector<float> gpuFrameTimes;

        uint64_t memoryUsage;
        uint64_t voxels;
        uint64_t nodes;

        glm::uvec3 dimensions;
    };

  public:
    static PerformanceLogger* getLogger()
    {
        static PerformanceLogger logger;
        return &logger;
    }

    std::function<void(const Event& event)> getFrameEvent()
    {
        return std::bind(&PerformanceLogger::frameEvent, this, std::placeholders::_1);
    }

    void init(Camera* camera);

    void addGPUTime(float GPUTime);

  private:
    PerformanceLogger();

    void frameEvent(const Event& event);
    void UI();
    void update(float delta);

    void getEntries();

    void startLog(std::filesystem::path file);

    void parseJson(std::filesystem::path file);

    PerfEntry parseEntry(const nlohmann::json& json, bool defaults = false);
    CameraSettings parseCamera(const nlohmann::json& json);

    void startPerf(const PerfEntry& perf);

    void savePerf();

  private:
    std::filesystem::path m_CurrentPath;
    std::filesystem::path m_Selected;

    Camera* m_Camera;

    PerfEntry m_Defaults;
    std::vector<CameraSettings> m_CameraSettings;
    std::vector<std::string> m_IDs;

    bool m_Running = false;
    std::string m_PerfName;
    std::vector<PerfEntry> m_PerfEntries;
    std::vector<Data> m_DataEntries;
    size_t m_CurrentEntry = 0;

    uint32_t m_CurrentCaptures = 0;
    uint32_t m_CurrentDelay = 0;

    std::vector<std::filesystem::path> m_FileEntries;
    std::vector<std::filesystem::path> m_Directories;
};
