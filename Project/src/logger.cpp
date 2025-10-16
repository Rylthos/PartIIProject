#include "logger.hpp"

#include "spdlog/sinks/ringbuffer_sink.h"
#include "spdlog/sinks/stdout_sinks.h"

#include <vector>

#include "imgui.h"

void Logger::init()
{
    std::vector<spdlog::sink_ptr> sinks;

    auto stdout_sink = std::make_shared<spdlog::sinks::stdout_sink_st>();
    stdout_sink->set_level(spdlog::level::debug);
    s_Ringbuffer = std::make_shared<spdlog::sinks::ringbuffer_sink_st>(256);
    s_Ringbuffer->set_level(spdlog::level::info);

    sinks.push_back(stdout_sink);
    sinks.push_back(s_Ringbuffer);

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
            if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar)) {
                std::vector<std::string> log_messages = s_Ringbuffer->last_formatted();

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
