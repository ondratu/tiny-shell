#include "wm/work_area.h"

#include <X11/Xatom.h>

#include <algorithm>

#include "display.h"

namespace tiny::wm {

static bool read_cardinals(::Window window, Atom property, unsigned long count,
                           long* values)
{
    Atom type;
    int format;
    unsigned long items;
    unsigned long after;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(tiny::get_display(), window, property, 0,
                           static_cast<long>(count), False, XA_CARDINAL, &type,
                           &format, &items, &after, &data) != Success ||
        data == nullptr) {
        return false;
    }
    const bool ok = format == 32 && items >= count;
    if (ok) {
        const auto* longs = reinterpret_cast<const long*>(data);
        std::copy(longs, longs + count, values);
    }
    XFree(data);
    return ok;
}

Struts read_struts(::Window window)
{
    const tiny::Display& dsp = tiny::get_display();
    long v[12] = {};
    if (!read_cardinals(window, dsp._NET_WM_STRUT_PARTIAL, 12, v) &&
        !read_cardinals(window, dsp._NET_WM_STRUT, 4, v)) {
        return {};
    }
    const auto edge = [](long value) {
        return static_cast<int>(std::clamp(value, 0L, 65535L));
    };
    return {edge(v[0]), edge(v[1]), edge(v[2]), edge(v[3])};
}

tiny::Rect compute_work_area(int screen_width, int screen_height,
                             const std::vector<Struts>& docks)
{
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
    for (const Struts& s : docks) {
        left = std::max(left, s.left);
        right = std::max(right, s.right);
        top = std::max(top, s.top);
        bottom = std::max(bottom, s.bottom);
    }
    // the docks cannot take the whole screen
    top = std::min(top, std::max(screen_height - 1, 0));
    bottom = std::min(bottom, std::max(screen_height - top - 1, 0));
    left = std::min(left, std::max(screen_width - 1, 0));
    right = std::min(right, std::max(screen_width - left - 1, 0));
    return {left, top, screen_width - left - right,
            screen_height - top - bottom};
}

tiny::Rect fit_in_area(tiny::Rect frame, const tiny::Rect& area)
{
    frame.width = std::min(frame.width, area.width);
    frame.height = std::min(frame.height, area.height);
    frame.x = std::clamp(frame.x, area.x, area.x + area.width - frame.width);
    frame.y = std::clamp(frame.y, area.y, area.y + area.height - frame.height);
    return frame;
}

} // namespace tiny::wm
