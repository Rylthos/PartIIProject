#pragma once

#include <glm/glm.hpp>

enum class EventFamily { KEYBOARD, MOUSE };

enum class KeyboardEventType { PRESS, RELEASE };
enum class MouseEventType { MOVE, ENTER_EXIT, CLICK, LIFT };

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
