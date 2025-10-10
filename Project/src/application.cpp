#include "application.hpp"

#include <iostream>

void Application::init()
{
    Logger::init();

    m_Window.init();
    initVulkan();

    LOG_INFO("Initialised application");
}

void Application::start() { std::cout << "Hello, World!\n"; }

void Application::cleanup() { LOG_INFO("Cleaup"); }

void Application::initVulkan() { }
