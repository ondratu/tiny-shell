#pragma once

#include <X11/Xlib.h>

#include <map>

typedef void (*handler_t)(const XEvent&, void*, void*);

class Handlers_t : public std::map<std::pair<int, Window>,
             std::tuple<handler_t, void*,void*>>
{
  public:
    void set_handler(int event_type, Window window,
                     handler_t handler, void* obj, void* data);

    void unset_handler(int event_type, Window window);
};
