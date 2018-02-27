#pragma once

#include <X11/Xlib.h>

#include <stdint.h>
#include <map>

namespace tiny {

class Object;

typedef void (Object::*event_signal_t)(const XEvent &e, void * data);

class Handlers : public std::map<
        std::pair<uint16_t, Window>,
        std::tuple<Object*, event_signal_t, void*>>
{
  public:
    void set_handler(uint16_t event_type, Window window,
            Object*o, event_signal_t signal, void * data);

    void unset_handler(uint16_t event_type, Window window);

    void call_hanlder(std::pair<uint16_t, Window> key, const XEvent &e);
};

typedef void (Object::*object_signal_t)(Object * o, const XEvent &e,
                                          void * data);

class Signal {
  public:
    Signal();

    void operator ()(Object * owner, const XEvent &e, void *data=nullptr);

    operator bool () const
    {
        return (object && method);
    }

    void connect(Object * object, object_signal_t method,
                 void *data=nullptr);

  private:
    Object *object;           // method owner
    object_signal_t method;     // method to call
    void * data;                // user data to send
};


class Object {
  public:
    Object()
    {}

    virtual ~Object()
    {}

  protected:
    void connect_window (uint16_t event_type, Window window,
            event_signal_t signal, void * data=nullptr);

    void disconnect_window (uint16_t event_type, Window window);
};

} // namespace tiny
