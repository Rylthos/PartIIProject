#pragma once

#include "events/events.hpp"
#include "glm/fwd.hpp"

#include <filesystem>
#include <functional>
#include <vector>

class PerformanceLogger {
    struct Defaults {
        uint32_t steps = 100;
        uint32_t captures = 10;
        std::string scene = "";
        glm::vec3 camera_pos = glm::vec3(0.f);
        float pitch = 0.0f;
        float yaw = 0.0f;
    };

  public:
    static PerformanceLogger* getLogger()
    {
        static PerformanceLogger logger;
        return &logger;
    }

    std::function<void(const Event& event)> getUIEvent()
    {
        return std::bind(&PerformanceLogger::UI, this, std::placeholders::_1);
    }

  private:
    PerformanceLogger();

    void UI(const Event& event);

    void getEntries();

    void startPerf(std::filesystem::path file);

    void parseJson();

  private:
    std::filesystem::path m_CurrentPath;
    std::filesystem::path m_Selected;

    Defaults m_Defaults;

    std::vector<std::filesystem::path> m_FileEntries;
    std::vector<std::filesystem::path> m_Directories;
};
