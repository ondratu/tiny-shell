#include <algorithm>

#include "icon_grid.h"
#include "x_util.h"

namespace tiny {

namespace {

constexpr size_t no_cell = static_cast<size_t>(-1);

// The text that fits in the width, shortened with dots when it does not.
std::string fit_text(::Display* display, XftFont* font, const std::string& text,
                     int max_width)
{
    const auto width_of = [&](const std::string& s) {
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font,
                           reinterpret_cast<const FcChar8*>(s.c_str()),
                           static_cast<int>(s.size()), &extents);
        return extents.xOff;
    };
    if (width_of(text) <= max_width) {
        return text;
    }
    std::string cut = text;
    while (!cut.empty()) {
        // remove the whole UTF-8 character, not only its last byte
        while ((static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80 &&
               cut.size() > 1) {
            cut.pop_back();
        }
        cut.pop_back();
        if (width_of(cut + "...") <= max_width) {
            break;
        }
    }
    return cut + "...";
}

} // namespace

IconGrid::IconGrid(int cols, int rows_arg, int cell_w, int cell_h, int icon,
                   int dot):
    Widget(Type::Normal, std::max(1, cols) * std::max(1, cell_w),
           height_of(rows_arg, cell_h, dot)),
    columns(static_cast<size_t>(std::max(1, cols))),
    rows(static_cast<size_t>(std::max(1, rows_arg))),
    cell_width(std::max(1, cell_w)),
    cell_height(std::max(1, cell_h)),
    icon_size(std::max(1, icon)),
    dot_size(std::max(1, dot)),
    pressed_cell(no_cell)
{
    name = "icongrid";
}

int IconGrid::height_of(int rows_arg, int cell_h, int dot)
{
    return std::max(1, rows_arg) * std::max(1, cell_h) + 3 * std::max(1, dot);
}

IconGrid::~IconGrid()
{
    if (event_done) {
        disconnect(Expose);
        disconnect(MotionNotify);
        disconnect(ButtonPress);
        disconnect(ButtonRelease);
    }
}

void IconGrid::set_events(long mask)
{
    Widget::set_events(mask | ExposureMask | PointerMotionMask |
                       ButtonPressMask | ButtonReleaseMask);

    connect(Expose, static_cast<event_signal_t>(&IconGrid::on_expose));
    connect(MotionNotify,
            static_cast<event_signal_t>(&IconGrid::on_motion_notify));
    connect(ButtonPress,
            static_cast<event_signal_t>(&IconGrid::on_button_press));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&IconGrid::on_button_release));
}

void IconGrid::set_geometry(int cols, int rows_arg, int cell_w, int cell_h,
                            int icon)
{
    columns = static_cast<size_t>(std::max(1, cols));
    rows = static_cast<size_t>(std::max(1, rows_arg));
    cell_width = std::max(1, cell_w);
    cell_height = std::max(1, cell_h);
    icon_size = std::max(1, icon);
    if (count > 0) {
        selected = std::min(selected, count - 1);
    }

    // the new size is known at once, the server tells it later
    width = static_cast<int>(columns) * cell_width;
    height = height_of(static_cast<int>(rows), cell_height, dot_size);
    if (is_realized) {
        XResizeWindow(display, window, x_dim(width), x_dim(height));
    }
    redraw();
}

void IconGrid::set_items(size_t count_arg, ItemFn item_of_arg)
{
    count = count_arg;
    item_of = std::move(item_of_arg);
    selected = 0;
    redraw();
}

size_t IconGrid::page_count() const
{
    return std::max<size_t>(1, (count + per_page() - 1) / per_page());
}

void IconGrid::select(size_t cell)
{
    if (count == 0) {
        return;
    }
    selected = std::min(cell, count - 1);
    redraw();
}

void IconGrid::select_down()
{
    if (count > 0 && selected / columns < (count - 1) / columns) {
        select(selected + columns); // the last cell when the row is shorter
    }
}

void IconGrid::select_up()
{
    if (selected >= columns) {
        select(selected - columns);
    }
}

void IconGrid::select_page(bool down)
{
    if (down && get_page() + 1 < page_count()) {
        select(selected + per_page());
    } else if (!down && get_page() > 0) {
        select(selected - per_page());
    }
}

size_t IconGrid::cell_at(int x, int y) const
{
    if (x < 0 || y < 0 || y >= static_cast<int>(rows) * cell_height) {
        return count;
    }
    const auto column = static_cast<size_t>(x / cell_width);
    if (column >= columns) {
        return count;
    }
    const size_t cell = get_page() * per_page() +
                        static_cast<size_t>(y / cell_height) * columns + column;
    return cell < count ? cell : count;
}

// The dots are a dot_size apart, closer when there are too many of them
int IconGrid::dots_step() const
{
    const int pages = static_cast<int>(page_count());
    if (pages < 2) {
        return 2 * dot_size;
    }
    return std::max(1,
                    std::min(2 * dot_size, (width - dot_size) / (pages - 1)));
}

// The line of dots is centered
int IconGrid::dots_left() const
{
    const int pages = static_cast<int>(page_count());
    return (width - ((pages - 1) * dots_step() + dot_size)) / 2;
}

size_t IconGrid::dot_at(int x, int y) const
{
    const int top = static_cast<int>(rows) * cell_height;
    const size_t pages = page_count();
    // every dot owns the space up to the half of the gap on both its sides
    const int from = x - dots_left() + (dots_step() - dot_size) / 2;
    if (pages < 2 || y < top || y >= height || from < 0) {
        return pages;
    }
    const auto slot = static_cast<size_t>(from / dots_step());
    return slot < pages ? slot : pages;
}

void IconGrid::redraw()
{
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void IconGrid::on_expose(const XEvent&, void*)
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

    const XRenderPictFormat* format = XRenderFindVisualFormat(display, visual);
    const Picture target =
            format ? XRenderCreatePicture(display, window, format, 0, nullptr)
                   : None;

    const int padding = get_padding();
    const int text_height = font->ascent + font->descent;
    // the icon and the text are together in the middle of the cell
    const int content = icon_size + padding / 2 + text_height;
    const int top = (cell_height - content) / 2;

    const size_t first = get_page() * per_page();
    const size_t last = std::min(count, first + per_page());
    for (size_t cell = first; item_of && cell < last; ++cell) {
        const size_t at = cell - first;
        const int x = static_cast<int>(at % columns) * cell_width;
        const int y = static_cast<int>(at / columns) * cell_height;
        const bool is_chosen = cell == selected;
        const Item item = item_of(cell);

        if (is_chosen) {
            XSetForeground(display, gc, style.get_bg(chosen));
            XFillRectangle(display, window, gc, x + 2, y + 2,
                           x_dim(cell_width - 4), x_dim(cell_height - 4));
        }
        if (item.icon != None && target != None) {
            const int w = item.icon_width > 0 ? item.icon_width : icon_size;
            const int h = item.icon_height > 0 ? item.icon_height : icon_size;
            XRenderComposite(display, PictOpOver, item.icon, None, target, 0, 0,
                             0, 0, x + (cell_width - w) / 2,
                             y + top + (icon_size - h) / 2, x_dim(w), x_dim(h));
        }
        const std::string text =
                fit_text(display, font, item.text, cell_width - padding);
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font,
                           reinterpret_cast<const FcChar8*>(text.c_str()),
                           static_cast<int>(text.size()), &extents);
        XftDrawStringUtf8(draw, &colors[is_chosen ? 1 : 0], font,
                          x + (cell_width - extents.xOff) / 2,
                          y + top + icon_size + padding / 2 + font->ascent,
                          reinterpret_cast<const FcChar8*>(text.c_str()),
                          static_cast<int>(text.size()));
    }

    const size_t pages = page_count();
    if (pages > 1) {
        const int y = static_cast<int>(rows) * cell_height + dot_size;
        for (size_t page = 0; page < pages; ++page) {
            XSetForeground(display, gc,
                           page == get_page() ? style.get_fg(chosen)
                                              : style.get_bg(chosen));
            XFillArc(display, window, gc,
                     dots_left() + static_cast<int>(page) * dots_step(), y,
                     x_uint(dot_size), x_uint(dot_size), 0, 360 * 64);
        }
    }

    if (target != None) {
        XRenderFreePicture(display, target);
    }
    XFreeGC(display, gc);
    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &colors[0]);
    XftColorFree(display, visual, colormap, &colors[1]);
}

void IconGrid::on_motion_notify(const XEvent& e, void*)
{
    const size_t cell = cell_at(e.xmotion.x, e.xmotion.y);
    if (cell < count && cell != selected) {
        select(cell);
    }
}

void IconGrid::on_button_press(const XEvent& e, void*)
{
    switch (e.xbutton.button) {
        case Button1: {
            pressed_cell = no_cell;
            const size_t page = dot_at(e.xbutton.x, e.xbutton.y);
            if (page < page_count()) {
                select(page * per_page());
                break;
            }
            pressed_cell = cell_at(e.xbutton.x, e.xbutton.y);
            if (pressed_cell < count) {
                select(pressed_cell);
            }
            break;
        }
        case Button4: // wheel up
            select_page(false);
            break;
        case Button5: // wheel down
            select_page(true);
            break;
        default:
            break;
    }
}

void IconGrid::on_button_release(const XEvent& e, void*)
{
    if (e.xbutton.button != Button1) {
        return;
    }
    const size_t cell = cell_at(e.xbutton.x, e.xbutton.y);
    const bool clicked = cell < count && cell == pressed_cell;
    pressed_cell = no_cell;
    if (clicked) {
        on_activate(this, e);
    }
}

} // namespace tiny
