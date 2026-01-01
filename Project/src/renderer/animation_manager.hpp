#pragma once

#include "events/events.hpp"

class AnimationManager {
  public:
    static AnimationManager* getManager()
    {
        static AnimationManager manager;
        return &manager;
    }

    std::function<void(const Event& event)> getFrameEvent()
    {
        return std::bind(&AnimationManager::frameEvent, this, std::placeholders::_1);
    }

    void reset();

  private:
    AnimationManager() { }

    void frameEvent(const Event& event);
    void UI();

    void update(float delta);

    void imguiDisable();
    void imguiEnable();

  private:
    uint32_t m_FPS = 4;

    bool m_Playing = false;
    bool m_Paused = false;

    uint32_t m_CachedFrame = 0;
};
