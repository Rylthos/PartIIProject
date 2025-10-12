#include "events.hpp"

void EventWatcher::subscribe(const EventFamily& family, EventFunction&& function)
{
    m_Observers[family].push_back(function);
}

void EventWatcher::post(const Event& event)
{
    auto family = event.family();
    if (m_Observers.find(family) == m_Observers.end())
        return;

    auto&& observers = m_Observers.at(family);
    for (auto&& observer : observers) {
        observer(event);
    }
}
