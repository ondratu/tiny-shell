#include "wm_object.h"

WMHandlers wm_handlers;

void WMHandlers::set_handler(
        int event_type, Window window,
        WMObject * o, event_signal_t signal, void* data)
{
    auto key = std::make_pair(event_type, window);
    auto val = std::make_tuple(o, signal, data);
    (*this)[key] = val;
}

void WMHandlers::unset_handler(int event_type, Window window)
{
    auto key = std::make_pair(event_type, window);
    this->erase(key);
}

void WMHandlers::call_hanlder(std::pair<int, Window> key, const XEvent &e)
{
    auto val = (*this)[key];
    WMObject * o = std::get<0>(val);
    event_signal_t signal = std::get<1>(val);
    (o->*signal)(e, std::get<2>(val));
}


Signal::Signal():object(nullptr), method(nullptr), data(nullptr)
{}

void Signal::operator ()(WMObject * owner, const XEvent &e)
{
    if (object && method){
        (object->*method)(owner, e, data);
    }
}

void Signal::connect(WMObject * object, object_signal_t method, void * data)
{
    this->object = object;
    this->method = method;
    this->data = data;
}


void WMObject::connect_window(int event_type, Window window,
        event_signal_t signal, void * data)
{
    wm_handlers.set_handler(event_type, window, this, signal, data);
}

void WMObject::disconnect_window(int event_type, Window window)
{
    wm_handlers.unset_handler(event_type, window);
}
