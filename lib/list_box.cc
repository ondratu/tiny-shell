#include <algorithm>

#include "list_box.h"
#include "x_util.h"

namespace tiny {

namespace {

constexpr size_t no_row = static_cast<size_t>(-1);

constexpr size_t wheel_rows = 3;

} // namespace

ListBox::ListBox(int w, int h, int row_h, int icon):
    Widget(Type::Normal, w, h),
    row_height(std::max(1, row_h)),
    icon_size(std::max(1, icon)),
    pressed_row(no_row)
{
    name = "listbox";
}

ListBox::~ListBox()
{
    if (event_done) {
        disconnect(Expose);
        disconnect(MotionNotify);
        disconnect(ButtonPress);
        disconnect(ButtonRelease);
    }
}

void ListBox::set_events(long mask)
{
    Widget::set_events(mask | ExposureMask | PointerMotionMask |
                       ButtonPressMask | ButtonReleaseMask);

    connect(Expose, static_cast<event_signal_t>(&ListBox::on_expose));
    connect(MotionNotify,
            static_cast<event_signal_t>(&ListBox::on_motion_notify));
    connect(ButtonPress,
            static_cast<event_signal_t>(&ListBox::on_button_press));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&ListBox::on_button_release));
}

void ListBox::set_items(size_t count_arg, ItemFn item_of_arg)
{
    count = count_arg;
    item_of = std::move(item_of_arg);
    selected = 0;
    first = 0;
    redraw();
}

size_t ListBox::visible_rows() const
{
    return std::max<size_t>(1, static_cast<size_t>(height / row_height));
}

void ListBox::select(size_t row)
{
    if (count == 0) {
        return;
    }
    selected = std::min(row, count - 1);
    if (selected < first) {
        first = selected;
    } else if (selected >= first + visible_rows()) {
        first = selected + 1 - visible_rows();
    }
    redraw();
}

void ListBox::select_page(bool down)
{
    const size_t rows = visible_rows();
    if (down) {
        select(selected + rows);
    } else {
        select(selected > rows ? selected - rows : 0);
    }
}

size_t ListBox::row_at(int y) const
{
    if (y < 0) {
        return count;
    }
    const size_t row = first + static_cast<size_t>(y / row_height);
    return row < count ? row : count;
}

void ListBox::scroll_to(size_t new_first)
{
    const size_t rows = visible_rows();
    first = std::min(new_first, count > rows ? count - rows : 0);
    // the selected row stays in view, so Return never starts a hidden one
    if (selected < first) {
        selected = first;
    } else if (selected >= first + rows) {
        selected = first + rows - 1;
    }
    redraw();
}

void ListBox::redraw()
{
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void ListBox::on_expose(const XEvent&, void*)
{
    const int screen = XDefaultScreen(display);
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);
    const Style& style = get_style();
    XftFont* font = style.get_font();

    const auto normal = static_cast<uint8_t>(Style::State::Normal);
    const auto chosen = static_cast<uint8_t>(
            normal | static_cast<uint8_t>(Style::State::Selected));

    XftColor colors[2];
    XftColorAllocName(display, visual, colormap,
                      style.get_xft_fg(normal).data(), &colors[0]);
    XftColorAllocName(display, visual, colormap,
                      style.get_xft_fg(chosen).data(), &colors[1]);
    XftDraw* draw = XftDrawCreate(display, window, visual, colormap);

    XSetWindowBackground(display, window, style.get_bg(normal));
    XClearWindow(display, window);

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetForeground(display, gc, style.get_bg(chosen));

    const XRenderPictFormat* format = XRenderFindVisualFormat(display, visual);
    const Picture target =
            format ? XRenderCreatePicture(display, window, format, 0, nullptr)
                   : None;

    const int padding = get_padding();
    const size_t last = std::min(count, first + visible_rows());
    for (size_t row = first; item_of && row < last; ++row) {
        const int y = static_cast<int>(row - first) * row_height;
        const bool is_chosen = row == selected;
        const Item item = item_of(row);

        if (is_chosen) {
            XFillRectangle(display, window, gc, 0, y, x_dim(width),
                           x_dim(row_height));
        }
        if (item.icon != None && target != None) {
            const int w = item.icon_width > 0 ? item.icon_width : icon_size;
            const int h = item.icon_height > 0 ? item.icon_height : icon_size;
            XRenderComposite(display, PictOpOver, item.icon, None, target, 0, 0,
                             0, 0, padding + (icon_size - w) / 2,
                             y + (row_height - h) / 2, x_dim(w), x_dim(h));
        }
        XftDrawStringUtf8(draw, &colors[is_chosen ? 1 : 0], font,
                          padding + icon_size + padding,
                          y + (row_height + font->ascent - font->descent) / 2,
                          reinterpret_cast<const FcChar8*>(item.text.c_str()),
                          static_cast<int>(item.text.size()));
    }

    if (target != None) {
        XRenderFreePicture(display, target);
    }
    XFreeGC(display, gc);
    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &colors[0]);
    XftColorFree(display, visual, colormap, &colors[1]);
}

void ListBox::on_motion_notify(const XEvent& e, void*)
{
    const size_t row = row_at(e.xmotion.y);
    if (row < count && row != selected) {
        select(row);
    }
}

void ListBox::on_button_press(const XEvent& e, void*)
{
    switch (e.xbutton.button) {
        case Button1:
            pressed_row = row_at(e.xbutton.y);
            if (pressed_row < count) {
                select(pressed_row);
            }
            break;
        case Button4: // wheel up
            scroll_to(first > wheel_rows ? first - wheel_rows : 0);
            break;
        case Button5: // wheel down
            scroll_to(first + wheel_rows);
            break;
        default:
            break;
    }
}

void ListBox::on_button_release(const XEvent& e, void*)
{
    if (e.xbutton.button != Button1) {
        return;
    }
    const size_t row = row_at(e.xbutton.y);
    const bool clicked = row < count && row == pressed_row;
    pressed_row = no_row;
    if (clicked) {
        on_activate(this, e);
    }
}

} // namespace tiny
