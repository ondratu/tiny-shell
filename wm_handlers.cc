#include "wm_handlers.h"

Handlers_t handlers;

void Handlers_t::set_handler(int event_type, Window window,
                             handler_t handler, void* obj, void* data)
{
    auto key = std::make_pair(event_type, window);
    auto val = std::make_tuple(handler, obj, data);
    (*this)[key] = val;
}

void Handlers_t::unset_handler(int event_type, Window window)
{
    auto key = std::make_pair(event_type, window);
    this->erase(key);
}
