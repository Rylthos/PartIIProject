#include "event_dispatcher.hpp"

void EventDispatcher::subscribe(const EventFamily& family, EventFunction&& function)
{
    m_Observers[family].push_back(function);
}

void EventDispatcher::post(const Event& event)
{
    auto family = event.family();
    if (m_Observers.find(family) == m_Observers.end())
        return;

    auto&& observers = m_Observers.at(family);
    for (auto&& observer : observers) {
        observer(event);
    }
}
