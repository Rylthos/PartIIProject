#pragma once

#include "events/events.hpp"

class AnimationManager {
  public:
    static AnimationManager* getManager()
    {
        static AnimationManager manager;
        return &manager;
    }

    std::function<void(const Event& event)> getUIEvent()
    {
        return std::bind(&AnimationManager::UI, this, std::placeholders::_1);
    }

  private:
    AnimationManager() { }

    void UI(const Event& event);
};
