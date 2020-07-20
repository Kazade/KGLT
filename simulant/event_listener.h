#pragma once

#include <list>
#include "types.h"
#include "keycodes.h"
#include "macros.h"

namespace smlt {

/* EventListeners can directly handle events received by the window
 * (e.g. input)
 *
 *  An EventListener must be properly registered/unregistered with the window
 * with core->register_event_listener() and core->unregister_event_listener()
 */

typedef uint32_t TouchPointID;

enum TouchEventType {
    TOUCH_EVENT_TYPE_FINGER_DOWN,
    TOUCH_EVENT_TYPE_FINGER_UP,
    TOUCH_EVENT_TYPE_FINGER_MOVE
};

struct TouchEvent {
    TouchEventType type;
    TouchPointID touch_id;
    Vec2 coord;
    Vec2 normalized_coord;
    Vec2 movement;
    float pressure;
};

enum KeyEventType {
    KEY_EVENT_TYPE_KEY_DOWN,
    KEY_EVENT_TYPE_KEY_UP
};

struct ModifierKeyState {
    bool lshift = false;
    bool rshift = false;
    bool lctrl = false;
    bool rctrl = false;
    bool lalt = false;
    bool ralt = false;
    bool lsuper = false;
    bool rsuper = false;
    bool num_lock = false;
    bool caps_lock = false;
    bool mode = false; // AltGr

    bool ctrl() const { return lctrl || rctrl; }
    bool shift() const { return lshift || rshift; }
    bool alt() const { return lalt || ralt; }
    bool super() const { return lsuper || rsuper; }
};

struct KeyEvent {
    KeyEventType type;
    KeyboardCode keyboard_code;
    ModifierKeyState modifiers;
};

class EventListener {
public:
    EventListener() {}
    virtual ~EventListener() {}

    void handle_touch_begin(Core* core, TouchPointID touch_id, float normalized_x, float normalized_y, float pressure);
    void handle_touch_end(Core* core, TouchPointID touch_id, float normalized_x, float normalized_y);
    void handle_touch_move(Core* core, TouchPointID touch_id, float normalized_x, float normalized_y, float dx, float dy);

    void handle_key_down(Core* core, KeyboardCode code, ModifierKeyState modifiers);
    void handle_key_up(Core* core, KeyboardCode code, ModifierKeyState modifiers);

private:
    virtual void on_key_down(const KeyEvent& evt) {
        _S_UNUSED(evt);
    }

    virtual void on_key_up(const KeyEvent& evt) {
        _S_UNUSED(evt);
    }

    virtual void on_touch_begin(const TouchEvent& evt) {
        _S_UNUSED(evt);
    }

    virtual void on_touch_end(const TouchEvent& evt) {
        _S_UNUSED(evt);
    }

    virtual void on_touch_move(const TouchEvent& evt) {
        _S_UNUSED(evt);
    }

    virtual void on_core_focus() {}
    virtual void on_core_blur() {}
    virtual void on_core_minimize() {}
    virtual void on_core_restore() {}
};

class EventListenerManager {
public:
    EventListenerManager() {}
    virtual ~EventListenerManager() {}

    void register_event_listener(EventListener* listener);
    void unregister_event_listener(EventListener* listener);

    void each_event_listener(std::function<void (EventListener*)> callback);

private:
    std::list<EventListener*> listeners_;
};

}
