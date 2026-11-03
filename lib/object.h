#pragma once

#include <X11/Xlib.h>

#include <stdint.h>
#include <functional>
#include <map>
#include <set>

#define TINY_LOG(...) tiny::log(__FILE__, __func__, __LINE__, __VA_ARGS__)

namespace tiny {

class Object;

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

//! `owner` is the object that fires the signal; a signal of a class that is
//! not an Object (like `wm::Window`) passes nullptr.
class Signal {
  public:
    using Function =
            std::function<void(Object* owner, const XEvent& e, void* data)>;

    void operator()(Object* owner, const XEvent& e, void* data = nullptr);

    operator bool() const { return static_cast<bool>(callback); }

    //! Call the function with the owner of the signal and the user data
    void connect(Function function);

    //! Call the function with the event only
    void connect(std::function<void(const XEvent& e)> function);

  private:
    Function callback;
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

    void disconnect_window(uint16_t event_type, ::Window window);

  private:
    EventConnections connections;
};

void log(const char* file, const char* function, uint32_t line,
         const char* format, ...) __attribute__((format(printf, 4, 5)));

void error(const char* format, ...) __attribute__((format(printf, 1, 2)));
} // namespace tiny
