#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "events/events.hpp"

class EventDispatcher {
    using EventFunction = std::function<void(const Event&)>;

  public:
    EventDispatcher() = default;
    virtual ~EventDispatcher() { }

    virtual void subscribe(const EventFamily& family, EventFunction&& function);
    virtual void post(const Event& event);

  protected:
    std::unordered_map<EventFamily, std::vector<EventFunction>> m_Observers;
};
