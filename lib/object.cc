#include <cstdlib>
#include <sys/time.h>
#include <cstdarg>
#include <cstdio>

#include "object.h"

namespace tiny {

Handlers handlers;

void Handlers::set_handler(uint16_t event_type, Window window,
                           event_function_t function)
{
    (*this)[std::make_pair(event_type, window)] = std::move(function);
}

void Handlers::unset_handler(uint16_t event_type, Window window)
{
    this->erase(std::make_pair(event_type, window));
}

void Handlers::call_handler(std::pair<uint16_t, Window> key, const XEvent& e)
{
    const auto it = this->find(key);
    if (it == this->end()) {
        return;
    }
    // a copy: the handler may unregister itself
    const event_function_t function = it->second;
    function(e);
}

Signal::Signal(): object(nullptr), method(nullptr), data(nullptr) {}

void Signal::operator()(Object* owner, const XEvent& e, void* event_data)
{
    if (owner_callback) {
        owner_callback(owner, e, (event_data ? event_data : this->data));
    } else if (callback) {
        callback(e);
    } else if (object && method) {
        (object->*method)(owner, e, (event_data ? event_data : this->data));
    }
}

void Signal::connect(std::function<void(const XEvent& e)> function)
{
    object = nullptr;
    method = nullptr;
    data = nullptr;
    owner_callback = nullptr;
    callback = std::move(function);
}

void Signal::connect(
        std::function<void(Object*, const XEvent&, void*)> function)
{
    object = nullptr;
    method = nullptr;
    data = nullptr;
    callback = nullptr;
    owner_callback = std::move(function);
}

void Signal::connect(Object* object_arg, object_signal_t method_arg,
                     void* data_arg)
{
    callback = nullptr;
    owner_callback = nullptr;
    this->object = object_arg;
    this->method = method_arg;
    this->data = data_arg;
}

EventConnections::~EventConnections()
{
    while (!connected_events.empty()) {
        const auto key = connected_events.crbegin();
        disconnect(key->first, key->second);
    }
}

void EventConnections::connect(uint16_t event_type, Window window,
                               event_function_t function)
{
    handlers.set_handler(event_type, window, std::move(function));
    connected_events.insert(std::make_pair(event_type, window));
}

void EventConnections::disconnect(uint16_t event_type, Window window)
{
    handlers.unset_handler(event_type, window);
    connected_events.erase(std::make_pair(event_type, window));
}

Object::~Object() = default;

void Object::connect_window(uint16_t event_type, Window window,
                            event_function_t function)
{
    connections.connect(event_type, window, std::move(function));
}

void Object::connect_window(uint16_t event_type, Window window,
                            event_signal_t signal, void* data)
{
    connect_window(event_type, window, [this, signal, data](const XEvent& e) {
        (this->*signal)(e, data);
    });
}

void Object::disconnect_window(uint16_t event_type, Window window)
{
    connections.disconnect(event_type, window);
}

void log(const char* file, const char* function, uint32_t line,
         const char* format, ...)
{
    struct timeval tp;
    gettimeofday(&tp, nullptr);
    //int64_t time_stamp = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    char* buffer = nullptr;
    va_list args;
    va_start(args, format);
    const int length = vasprintf(&buffer, format, args);
    va_end(args);
    if (length < 0) {
        return;
    }

    // time_t and suseconds_t differ between systems, e.g. NetBSD has an int
    fprintf(stderr, "%lld.%06lld in %s: %s (%s:%u)\n",
            static_cast<long long>(tp.tv_sec),
            static_cast<long long>(tp.tv_usec), function, buffer, file, line);
    free(buffer);
}

void error(const char* format, ...)
{
    struct timeval tp;
    gettimeofday(&tp, nullptr);
    //int64_t time_stamp = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    char* buffer = nullptr;
    va_list args;
    va_start(args, format);
    const int length = vasprintf(&buffer, format, args);
    va_end(args);
    if (length < 0) {
        return;
    }

    fprintf(stderr, "%lld.%06lld %s\n", static_cast<long long>(tp.tv_sec),
            static_cast<long long>(tp.tv_usec), buffer);
    free(buffer);
}

} // namespace tiny
