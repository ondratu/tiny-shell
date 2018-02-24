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

void Signal::operator ()(Object * owner, const XEvent &e)
{
    if (object && method){
        (object->*method)(owner, e, data);
    }
}

void Signal::connect(Object * object, object_signal_t method, void * data)
{
    this->object = object;
    this->method = method;
    this->data = data;
}


void Object::connect_window(uint16_t event_type, Window window,
        event_signal_t signal, void * data)
{
    handlers.set_handler(event_type, window, this, signal, data);
}

void Object::disconnect_window(uint16_t event_type, Window window)
{
    handlers.unset_handler(event_type, window);
}

} // namespace tiny
