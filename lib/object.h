#pragma once

#include <X11/Xlib.h>

#include <stdint.h>
#include <functional>
#include <map>
#include <set>

#define TINY_LOG(...) tiny::log(__FILE__, __func__, __LINE__, __VA_ARGS__)

namespace tiny {

class Object;

using event_signal_t = void (Object::*)(const XEvent& e, void* data);

using event_function_t = std::function<void(const XEvent& e)>;

class Handlers
    : public std::map<std::pair<uint16_t, ::Window>, event_function_t> {
  public:
    void set_handler(uint16_t event_type, ::Window window,
                     event_function_t function);

    void unset_handler(uint16_t event_type, ::Window window);

    //! Call the handler of the key; an event without a handler is ignored
    void call_handler(std::pair<uint16_t, ::Window> key, const XEvent& e);
};

using object_signal_t = void (Object::*)(Object* o, const XEvent& e,
                                         void* data);

//! `owner` is the object that fires the signal; a signal of a class that is
//! not an Object (like `wm::Window`) passes nullptr.
class Signal {
  public:
    Signal();

    void operator()(Object* owner, const XEvent& e, void* data = nullptr);

    operator bool() const
    {
        return (object && method) || callback || owner_callback;
    }

    void connect(Object* object, object_signal_t method, void* data = nullptr);

    //! Call the function instead of a method of an object
    void connect(std::function<void(const XEvent& e)> function);

    //! Call the function with the owner of the signal and the user data
    void connect(std::function<void(Object* owner, const XEvent& e, void* data)>
                         function);

  private:
    Object* object;         // method owner
    object_signal_t method; // method to call
    void* data;             // user data to send
    std::function<void(const XEvent& e)> callback;
    std::function<void(Object* owner, const XEvent& e, void* data)>
            owner_callback;
};

/**
 * @short Event handlers of one owner, unregistered together with it.
 *
 * It is a plain member (not a base class), so a class with more bases can
 * have it once. The handler table knows only functions, never the owner.
 */
class EventConnections {
  public:
    EventConnections() = default;
    EventConnections(const EventConnections&) = delete;
    EventConnections& operator=(const EventConnections&) = delete;

    ~EventConnections();

    void connect(uint16_t event_type, ::Window window,
                 event_function_t function);

    void disconnect(uint16_t event_type, ::Window window);

  private:
    std::set<std::pair<uint16_t, ::Window>> connected_events;
};

class Object {
  public:
    Object() = default;

    virtual ~Object();

  protected:
    void connect_window(uint16_t event_type, ::Window window,
                        event_function_t function);

    //! The method of this object (a cast of a pointer to a member of a
    //! derived class is not safe with several bases, prefer a lambda)
    void connect_window(uint16_t event_type, ::Window window,
                        event_signal_t signal, void* data = nullptr);

    void disconnect_window(uint16_t event_type, ::Window window);

  private:
    EventConnections connections;
};

void log(const char* file, const char* function, uint32_t line,
         const char* format, ...) __attribute__((format(printf, 4, 5)));

void error(const char* format, ...) __attribute__((format(printf, 1, 2)));
} // namespace tiny
