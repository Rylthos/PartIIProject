#pragma once

#include <glm/glm.hpp>

enum class EventFamily { KEYBOARD, MOUSE, WINDOW, FRAME };

enum class KeyboardEventType { PRESS, RELEASE };
enum class MouseEventType { MOVE, ENTER_EXIT, CLICK, LIFT };
enum class WindowEventType { RESIZE };
enum class FrameEventType { RENDER, UPDATE, UI };

class Event {
  public:
    Event() = default;
    virtual ~Event() = default;

    virtual EventFamily family() const = 0;
};

class KeyboardEvent : public Event {
  public:
    virtual EventFamily family() const { return EventFamily::KEYBOARD; }
    virtual KeyboardEventType type() const = 0;
};

class KeyboardPressEvent : public KeyboardEvent {
  public:
    KeyboardPressEvent() = default;
    ~KeyboardPressEvent() = default;

    virtual KeyboardEventType type() const { return KeyboardEventType::PRESS; }

  public:
    int keycode;
    int mods;
};

class KeyboardReleaseEvent : public KeyboardEvent {
  public:
    KeyboardReleaseEvent() = default;
    ~KeyboardReleaseEvent() = default;

    virtual KeyboardEventType type() const { return KeyboardEventType::RELEASE; }

  public:
    int keycode;
    int mods;
};

class MouseEvent : public Event {
  public:
    virtual EventFamily family() const { return EventFamily::MOUSE; }
    virtual MouseEventType type() const = 0;
};

class MouseMoveEvent : public MouseEvent {
  public:
    MouseMoveEvent() = default;
    ~MouseMoveEvent() = default;

    virtual MouseEventType type() const { return MouseEventType::MOVE; }

  public:
    glm::vec2 position;
    glm::vec2 delta;
};

class MouseEnterExitEvent : public MouseEvent {
  public:
    MouseEnterExitEvent() = default;
    ~MouseEnterExitEvent() = default;

    virtual MouseEventType type() const { return MouseEventType::ENTER_EXIT; }

  public:
    bool entered;
};

class MouseClickEvent : public MouseEvent {
  public:
    MouseClickEvent() = default;
    ~MouseClickEvent() = default;

    virtual MouseEventType type() const { return MouseEventType::CLICK; }

  public:
    int button;
};

class MouseLiftEvent : public MouseEvent {
  public:
    MouseLiftEvent() = default;
    ~MouseLiftEvent() = default;

    virtual MouseEventType type() const { return MouseEventType::LIFT; }

  public:
    int button;
};

class WindowEvent : public Event {
  public:
    virtual EventFamily family() const { return EventFamily::WINDOW; }
    virtual WindowEventType type() const = 0;
};

class WindowResizeEvent : public WindowEvent {
  public:
    WindowResizeEvent() = default;
    ~WindowResizeEvent() = default;

    virtual WindowEventType type() const { return WindowEventType::RESIZE; }

  public:
    glm::ivec2 newSize;
};

class FrameEvent : public Event {
  public:
    virtual EventFamily family() const { return EventFamily::FRAME; }
    virtual FrameEventType type() const = 0;
};

class RenderEvent : public FrameEvent {
  public:
    RenderEvent() = default;
    ~RenderEvent() = default;

    virtual FrameEventType type() const { return FrameEventType::RENDER; }
};

class UpdateEvent : public FrameEvent {
  public:
    UpdateEvent() = default;
    ~UpdateEvent() = default;

    virtual FrameEventType type() const { return FrameEventType::UPDATE; }

  public:
    float delta;
};

class UIEvent : public FrameEvent {
  public:
    UIEvent() = default;
    ~UIEvent() = default;

    virtual FrameEventType type() const { return FrameEventType::UI; }
};
