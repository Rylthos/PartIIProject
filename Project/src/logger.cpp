#include "logger.hpp"

#include "spdlog/common.h"
#include "spdlog/sinks/ringbuffer_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

#include <vector>

#include "imgui.h"

void Logger::init()
{
    std::vector<spdlog::sink_ptr> sinks;

    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    stdout_sink->set_level(spdlog::level::debug);
    s_RingBuffer = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(256);
    s_RingBuffer->set_level(spdlog::level::info);

    sinks.push_back(stdout_sink);
    sinks.push_back(s_RingBuffer);

    s_Logger = std::make_shared<spdlog::logger>("GeneralLogger", begin(sinks), end(sinks));

    s_Logger->set_pattern("[%d/%m/%C %H:%M:%S.%e] [%l] %v");
    s_Logger->set_level(spdlog::level::debug);

    LOG_DEBUG("Initialized Logger");
}

void Logger::UI(const Event& event)
{
    const FrameEvent& frameEvent = static_cast<const FrameEvent&>(event);

    if (frameEvent.type() == FrameEventType::UI) {
        if (ImGui::Begin("Logger")) {
            const uint32_t maxLevel = spdlog::level::n_levels;
            const char* levels[] = { "trace", "debug", "info", "warn", "error", "critical", "off" };
            static uint8_t currentLevel = spdlog::level::info;
            const char* previewValue = levels[currentLevel];

            ImGui::Text("Logging Level");
            ImGui::SameLine();
            ImGui::PushItemWidth(0.15 * ImGui::GetWindowWidth());
            if (ImGui::BeginCombo("##LoggingLevel", previewValue)) {
                for (uint32_t i = 0; i < maxLevel; i++) {
                    const bool isSelected = (currentLevel == i);
                    if (ImGui::Selectable(levels[i], isSelected)) {
                        currentLevel = i;

                        s_RingBuffer->set_level(
                            static_cast<spdlog::level::level_enum>(currentLevel));
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
                std::vector<std::string> log_messages = s_RingBuffer->last_formatted();

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                for (size_t i = 0; i < log_messages.size(); i++) {
                    ImGui::TextUnformatted(log_messages.at(i).c_str());
                }
                ImGui::PopStyleVar();

                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
}
