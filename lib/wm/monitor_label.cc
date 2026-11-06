#include "wm/monitor_label.h"

#include <X11/Xft/Xft.h>

#include <algorithm>

#include "style.h"
#include "wm/shell_theme.h"
#include "x_util.h"

namespace tiny::wm {

namespace {

//! The number is this many times as high as the panel (the label is small, it
//! only has to be seen from a distance)
constexpr int number_scale = 1;

//! A font for the big number, nothing when it can't be opened
XftFont* open_number_font(::Display* display)
{
    return XftFontOpen(display, XDefaultScreen(display), XFT_FAMILY,
                       XftTypeString, "sans", XFT_PIXEL_SIZE, XftTypeDouble,
                       static_cast<double>(shell_theme.panel * number_scale),
                       static_cast<char*>(nullptr));
}

int text_width(::Display* display, XftFont* font, const std::string& text)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

} // namespace

MonitorLabel::MonitorLabel(): tiny::Widget(1, 1)
{
    name = "wm_monitor_label";
}

MonitorLabel::~MonitorLabel()
{
    if (event_done) {
        disconnect(Expose);
    }
}

void MonitorLabel::set_events(long mask)
{
    Widget::set_events(ExposureMask | mask);

    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
}

void MonitorLabel::set_text(const std::string& number_arg,
                            const std::string& detail_arg)
{
    number = number_arg;
    detail = detail_arg;
}

void MonitorLabel::place(const tiny::Rect& area)
{
    XftFont* big = open_number_font(display);
    XftFont* small = get_style().get_font();
    const int padding = small->height;
    const int number_width = big ? text_width(display, big, number) : 0;
    const int detail_width = text_width(display, small, detail);
    const int number_height = big ? big->height : 0;
    if (big) {
        XftFontClose(display, big);
    }

    width = std::max(number_width, detail_width) + 2 * padding;
    height = number_height + small->height + 2 * padding;
    XMoveResizeWindow(display, window, area.x + padding, area.y + padding,
                      tiny::x_dim(width), tiny::x_dim(height));
}

void MonitorLabel::on_expose(const XEvent&, void*)
{
    const int screen = XDefaultScreen(display);
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);
    const uint8_t state = get_theme_state();
    XftFont* big = open_number_font(display);
    XftFont* small = get_style().get_font();
    const int padding = small->height;

    XftColor color;
    XftColorAllocName(display, visual, colormap,
                      get_style().get_xft_fg(state).data(), &color);
    XftDraw* draw = XftDrawCreate(display, window, visual, colormap);

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    int y = padding;
    if (big) {
        y += big->ascent;
        XftDrawStringUtf8(draw, &color, big, padding, y,
                          reinterpret_cast<const FcChar8*>(number.c_str()),
                          static_cast<int>(number.size()));
        y += big->descent;
    }
    XftDrawStringUtf8(draw, &color, small, padding, y + small->ascent,
                      reinterpret_cast<const FcChar8*>(detail.c_str()),
                      static_cast<int>(detail.size()));

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
    if (big) {
        XftFontClose(display, big);
    }
}

MonitorLabels::MonitorLabels() = default;

MonitorLabels::~MonitorLabels() = default;

std::string MonitorLabels::detail_of(const Monitor& monitor, bool primary)
{
    std::string text =
            monitor.name.empty() ? std::string("screen") : monitor.name;
    text += " " + std::to_string(monitor.area.width) + "x" +
            std::to_string(monitor.area.height);
    if (primary) {
        text += " (primary)";
    }
    return text;
}

void MonitorLabels::show(::Window root, const Monitors& monitors,
                         size_t primary, const std::vector<tiny::Rect>& areas)
{
    ::Display* display = tiny::get_display();
    // one label for each monitor: made when there are not enough, dropped when
    // there are too many
    while (labels.size() > monitors.size()) {
        labels.pop_back();
    }
    while (labels.size() < monitors.size()) {
        labels.push_back(std::make_unique<MonitorLabel>());
        MonitorLabel& label = *labels.back();
        label.realize(root, 0, 0);
        label.set_events();
        XSetWindowAttributes attrs;
        attrs.override_redirect = True; // ours, never framed
        XChangeWindowAttributes(display, label.get_window(), CWOverrideRedirect,
                                &attrs);
    }
    for (size_t i = 0; i < monitors.size(); ++i) {
        MonitorLabel& label = *labels[i];
        label.set_text(std::to_string(i + 1),
                       detail_of(monitors[i], i == primary));
        label.place(i < areas.size() ? areas[i] : monitors[i].area);
        label.map();
        XClearArea(display, label.get_window(), 0, 0, 0, 0, True);
    }
    raise();
}

bool MonitorLabels::owns(::Window window) const
{
    return std::any_of(labels.begin(), labels.end(), [window](const auto& l) {
        return l->get_window() == window;
    });
}

void MonitorLabels::hide()
{
    labels.clear();
}

void MonitorLabels::raise()
{
    for (const auto& label : labels) {
        XRaiseWindow(tiny::get_display(), label->get_window());
    }
}

} // namespace tiny::wm
