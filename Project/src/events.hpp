#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

enum class EventFamily { KEYBOARD, MOUSE, WINDOW };
enum class KeyboardEventType { PRESS, RELEASE };
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

class EventWatcher {
    using EventFunction = std::function<void(const Event&)>;

  public:
    EventWatcher() = default;
    virtual ~EventWatcher() { }

    virtual void subscribe(const EventFamily& family, EventFunction&& function);
    virtual void post(const Event& event);

  protected:
    std::unordered_map<EventFamily, std::vector<EventFunction>> m_Observers;
};
