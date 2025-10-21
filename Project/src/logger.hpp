#pragma once

#include "events.hpp"

#include <functional>

#include "spdlog/sinks/ringbuffer_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "vulkan/vk_enum_string_helper.h"

#include <memory>

class Logger {
  public:
    static void init();
    static std::shared_ptr<spdlog::logger> getLogger() { return s_Logger; }

    static std::function<void(const Event&)> getFrameEvent()
    {
        return std::bind(&Logger::UI, std::placeholders::_1);
    }

  private:
    inline static std::shared_ptr<spdlog::logger> s_Logger;
    inline static std::shared_ptr<spdlog::sinks::ringbuffer_sink_st> s_Ringbuffer;

    static void UI(const Event& event);

  private:
    Logger() = delete;
};

#ifdef DEBUG
// #if true
#define LOG_DEBUG(...) Logger::getLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::getLogger()->info(__VA_ARGS__)
#define LOG_ERROR(...) Logger::getLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::getLogger()->critical(__VA_ARGS__)

#define VK_CHECK(result, msg)                                                                      \
    do {                                                                                           \
        VkResult temp = (result);                                                                  \
        if (temp != VK_SUCCESS) {                                                                  \
            LOG_ERROR("{}: {}", (msg), string_VkResult(temp));                                     \
        }                                                                                          \
    } while (0)
#else
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_ERROR(...)
#define LOG_CRITICAL(...)
#define VK_CHECK(result, msg) result
#endif
