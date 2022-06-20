#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>

#include "object.h"

namespace tiny {

Handlers handlers;

void Handlers::set_handler(
        uint16_t event_type, Window window,
        Object * o, event_signal_t signal, void* data)
{
    auto key = std::make_pair(event_type, window);
    auto val = std::make_tuple(o, signal, data);
    (*this)[key] = val;
}

void Handlers::unset_handler(uint16_t event_type, Window window)
{
    auto key = std::make_pair(event_type, window);
    this->erase(key);
}

void Handlers::call_hanlder(
        std::pair<uint16_t, Window> key, const XEvent &e)
{
    auto val = (*this)[key];
    Object * o = std::get<0>(val);
    event_signal_t signal = std::get<1>(val);
    (o->*signal)(e, std::get<2>(val));
}


Signal::Signal():object(nullptr), method(nullptr), data(nullptr)
{}

void Signal::operator ()(Object *owner, const XEvent &e, void *data)
{
    if (object && method){
        (object->*method)(owner, e, (data ? data : this->data));
    }
}

void Signal::connect(Object *object, object_signal_t method, void *data)
{
    this->object = object;
    this->method = method;
    this->data = data;
}

Object::~Object()
{
    for(const auto &key :connected_events) {
        TINY_LOG("missing %d for %x", key.first, key.second);
        disconnect_window(key.first, key.second);
    }
    connected_events.clear();
}

void Object::connect_window(uint16_t event_type, Window window,
        event_signal_t signal, void * data)
{
    handlers.set_handler(event_type, window, this, signal, data);
    connected_events.insert(std::make_pair(event_type, window));
}

void Object::disconnect_window(uint16_t event_type, Window window)
{
    handlers.unset_handler(event_type, window);
    connected_events.erase(std::make_pair(event_type, window));
}

void log(const char *file, const char* function, uint32_t line,
        const char *format, ...)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    //int64_t time_stamp = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    char * buffer = NULL;
    va_list args;
    va_start(args, format);
    vasprintf(&buffer, format, args);
    va_end(args);

    fprintf(stderr, "%ld.%04ld in %s: %s (%s:%u)\n",
            tp.tv_sec, tp.tv_usec, function, buffer, file, line);
}

void error(const char *format, ...)
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    //int64_t time_stamp = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    char * buffer = NULL;
    va_list args;
    va_start(args, format);
    vasprintf(&buffer, format, args);
    va_end(args);

    fprintf(stderr, "%ld.%04ld %s\n", tp.tv_sec, tp.tv_usec, buffer);
}


} // namespace tiny
