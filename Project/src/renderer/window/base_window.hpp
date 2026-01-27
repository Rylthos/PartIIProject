#pragma once

#include "events/event_dispatcher.hpp"

#include "glm/glm.hpp"

class Window : public EventDispatcher {
  public:
    Window() = default;
    virtual ~Window() = default;

    virtual void init() = 0;
    virtual void cleanup() = 0;

    virtual bool shouldClose() = 0;
    virtual void requestClose() = 0;

    virtual glm::uvec2 getWindowSize() { return p_WindowSize; }

  protected:
    glm::uvec2 p_WindowSize { 0 };
};
