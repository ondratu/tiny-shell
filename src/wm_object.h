#pragma once

#include <X11/Xlib.h>

#include <map>

class WMObject;

typedef void (WMObject::*event_signal_t)(const XEvent &e, void * data);

class WMHandlers : public std::map<
        std::pair<int, Window>,
        std::tuple<WMObject*, event_signal_t, void*>>
{
  public:
    void set_handler(int event_type, Window window,
            WMObject*o, event_signal_t signal, void * data);

    void unset_handler(int event_type, Window window);

    void call_hanlder(std::pair<int, Window> key, const XEvent &e);
};

typedef void (WMObject::*object_signal_t)(WMObject * o, const XEvent &e,
                                          void * data);

class Signal {
  public:
    Signal();

    void operator ()(WMObject * owner, const XEvent &e);

    operator bool () const
    {
        return (object && method);
    }

    void connect(WMObject * object, object_signal_t method,
                 void * data = nullptr);

  private:
    WMObject *object;           // method owner
    object_signal_t method;     // method to call
    void * data;                // user data to send
};


class WMObject {
  public:
    WMObject()
    {}

    virtual ~WMObject()
    {}

  protected:
    void connect_window (int event_type, Window window,
            event_signal_t signal, void * data);

    void disconnect_window (int event_type, Window window);
};
