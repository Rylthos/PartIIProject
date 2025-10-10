#include "logger.hpp"

#include "spdlog/sinks/stdout_sinks.h"

#include <vector>

void Logger::init()
{
    std::vector<spdlog::sink_ptr> sinks;

    // Single threaded
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_st>());

    s_Logger = std::make_shared<spdlog::logger>("GeneralLogger", begin(sinks), end(sinks));

    s_Logger->set_pattern("[%d/%m/%C %H:%M:%S.%e] [%l] %v");

    LOG_INFO("Initialized Logger");
}
