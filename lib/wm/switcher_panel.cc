#include "wm/switcher_panel.h"

#include <algorithm>

#include "x_util.h"
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>

namespace tiny::wm {

SwitcherPanel::SwitcherPanel(int icon_size_arg, int cell_width_arg):
    tiny::Widget(1, 1), icon_size(icon_size_arg), cell_width(cell_width_arg)
{
    name = "switcher_panel";
    width = width_of(0);
    height = get_cell_height() + 2 * shell_theme.dock_border;
}

SwitcherPanel::~SwitcherPanel()
{
    if (event_done) {
        disconnect(Expose);
    }
    if (target != None) {
        XRenderFreePicture(display, target);
    }
}

void SwitcherPanel::set_events(long mask)
{
    tiny::Widget::set_events(ExposureMask | mask);
    connect(Expose, [this](const XEvent& e) { on_expose(e); });
}

int SwitcherPanel::get_cell_height() const
{
    const XftFont* font = tiny::theme.list.get_font(); // no virtual call here
    const int text = font ? font->ascent + font->descent : shell_theme.panel;
    const int gap = shell_theme.dock_border;
    // the icon, the dots, the text and a margin around them
    return gap + icon_size + shell_theme.dock_dot + gap + text + gap;
}

int SwitcherPanel::width_of(size_t cell_count) const
{
    return static_cast<int>(cell_count) * cell_width +
           2 * shell_theme.dock_border;
}

void SwitcherPanel::set_geometry(int icon_size_arg, int cell_width_arg)
{
    icon_size = icon_size_arg;
    cell_width = cell_width_arg;
    fit_window();
    redraw();
}

void SwitcherPanel::set_max_width(int value)
{
    max_width = value;
}

CellRange SwitcherPanel::get_visible() const
{
    const size_t room =
            max_width > 0
                    ? cells_that_fit(max_width - 2 * shell_theme.dock_border,
                                     cell_width)
                    : cells.size();
    return visible_cells(cells.size(), selected.value_or(0), room);
}

void SwitcherPanel::fit_window()
{
    width = width_of(get_visible().count);
    height = get_cell_height() + 2 * shell_theme.dock_border;
    if (is_realized) {
        XResizeWindow(display, window, x_dim(width), x_dim(height));
    }
}

void SwitcherPanel::set_cells(std::vector<SwitcherCell> list,
                              std::optional<size_t> cursor)
{
    cells = std::move(list);
    selected = cursor;
    fit_window();
    redraw();
}

void SwitcherPanel::select(std::optional<size_t> cell)
{
    selected = cell;
    fit_window();
    redraw();
}

void SwitcherPanel::place(int x_center, int y)
{
    if (is_realized) {
        XMoveWindow(display, window, x_center - width / 2, y);
    }
}

void SwitcherPanel::redraw()
{
    if (is_realized && is_maped) {
        XClearArea(display, window, 0, 0, 0, 0, True);
    }
}

void SwitcherPanel::draw_icon(const SwitcherCell& cell, int x, int y)
{
    if (cell.picture != None && target != None) {
        const int w = std::min(cell.icon_width, icon_size);
        const int h = std::min(cell.icon_height, icon_size);
        const bool scaled = cell.icon_width != icon_size;
        if (scaled) { // the transform maps the target to the source
            const double factor =
                    static_cast<double>(cell.icon_width) / icon_size;
            XTransform scale = {{{XDoubleToFixed(factor), 0, 0},
                                 {0, XDoubleToFixed(factor), 0},
                                 {0, 0, XDoubleToFixed(1)}}};
            XRenderSetPictureTransform(display, cell.picture, &scale);
        }
        const int shown_w = scaled ? icon_size : w;
        const int shown_h = scaled ? icon_size * cell.icon_height /
                                             std::max(1, cell.icon_width)
                                   : h;
        XRenderComposite(display, PictOpOver, cell.picture, None, target, 0, 0,
                         0, 0, x + (cell_width - shown_w) / 2,
                         y + (icon_size - shown_h) / 2, x_uint(shown_w),
                         x_uint(shown_h));
        if (scaled) {
            XTransform identity = {{{XDoubleToFixed(1), 0, 0},
                                    {0, XDoubleToFixed(1), 0},
                                    {0, 0, XDoubleToFixed(1)}}};
            XRenderSetPictureTransform(display, cell.picture, &identity);
        }
        return;
    }
    const auto state = static_cast<uint8_t>(Style::State::Normal);
    if (cell.pixmap != None) { // the legacy icon is not scaled
        GC gc = XCreateGC(display, window, 0, nullptr);
        if (cell.mask != None) {
            XGCValues values;
            values.clip_mask = cell.mask;
            values.clip_x_origin = x + (cell_width - shell_theme.icon) / 2;
            values.clip_y_origin = y + (icon_size - shell_theme.icon) / 2;
            XChangeGC(display, gc, GCClipMask | GCClipXOrigin | GCClipYOrigin,
                      &values);
        }
        XCopyArea(display, cell.pixmap, window, gc, 0, 0,
                  x_uint(shell_theme.icon), x_uint(shell_theme.icon),
                  x + (cell_width - shell_theme.icon) / 2,
                  y + (icon_size - shell_theme.icon) / 2);
        XFreeGC(display, gc);
        return;
    }
    if (symbols != nullptr) { // the generic icon of an application
        symbols->draw(window, "application-x-executable", icon_size * 2 / 3,
                      lighter(get_style().get_fg(state)), cell_width, icon_size,
                      x, y);
    }
}

void SwitcherPanel::draw_dots(GC gc, const SwitcherCell& cell, int x, int y)
{
    const int dot = shell_theme.dock_dot;
    const int step = 2 * dot;
    const size_t room =
            static_cast<size_t>(std::max(1, (cell_width + dot) / step));
    const int count = static_cast<int>(std::min(cell.dots, room));
    const int left = x + (cell_width - (count * step - dot)) / 2;
    XSetForeground(
            display, gc,
            get_style().get_fg(static_cast<uint8_t>(Style::State::Normal)));
    for (int i = 0; i < count; ++i) {
        XFillArc(display, window, gc, left + i * step, y, x_uint(dot),
                 x_uint(dot), 0, 360 * 64);
    }
}

void SwitcherPanel::on_expose(const XEvent&)
{
    const int screen = XDefaultScreen(display);
    Visual* visual = XDefaultVisual(display, screen);
    const Colormap colormap = XDefaultColormap(display, screen);
    const Style& style = get_style();
    XftFont* font = style.get_font();
    const auto normal = static_cast<uint8_t>(Style::State::Normal);
    const auto chosen = static_cast<uint8_t>(
            normal | static_cast<uint8_t>(Style::State::Selected));

    XSetWindowBackground(display, window, style.get_bg(normal));
    XClearWindow(display, window);
    if (target == None) { // the window has the default visual
        const XRenderPictFormat* format =
                XRenderFindVisualFormat(display, visual);
        if (format != nullptr) {
            target = XRenderCreatePicture(display, window, format, 0, nullptr);
        }
    }

    GC gc = XCreateGC(display, window, 0, nullptr);
    XftColor colors[2];
    XftDraw* draw = nullptr;
    if (font != nullptr) {
        XftColorAllocName(display, visual, colormap,
                          style.get_xft_fg(normal).data(), &colors[0]);
        XftColorAllocName(display, visual, colormap,
                          style.get_xft_fg(chosen).data(), &colors[1]);
        draw = XftDrawCreate(display, window, visual, colormap);
    }

    const int gap = shell_theme.dock_border;
    const int dot_row = shell_theme.dock_dot + gap;
    const CellRange visible = get_visible();
    for (size_t i = 0; i < visible.count; ++i) {
        const size_t index = visible.first + i;
        const SwitcherCell& cell = cells[index];
        const bool is_chosen = selected && *selected == index;
        const int x =
                shell_theme.dock_border + static_cast<int>(i) * cell_width;
        const int y = shell_theme.dock_border;
        if (is_chosen) {
            XSetForeground(display, gc, style.get_bg(chosen));
            XFillRectangle(display, window, gc, x, y, x_uint(cell_width),
                           x_uint(get_cell_height()));
        }
        draw_icon(cell, x, y + gap);
        if (cell.dots > 1) {
            draw_dots(gc, cell, x, y + gap + icon_size + gap / 2);
        }
        if (draw != nullptr) {
            const std::string text =
                    fit_text(display, font, cell.text, cell_width - 2 * gap);
            XGlyphInfo extents;
            XftTextExtentsUtf8(display, font,
                               reinterpret_cast<const FcChar8*>(text.c_str()),
                               static_cast<int>(text.size()), &extents);
            XftDrawStringUtf8(draw, &colors[is_chosen ? 1 : 0], font,
                              x + (cell_width - extents.xOff) / 2,
                              y + gap + icon_size + dot_row + font->ascent,
                              reinterpret_cast<const FcChar8*>(text.c_str()),
                              static_cast<int>(text.size()));
        }
    }
    if (draw != nullptr) {
        XftDrawDestroy(draw);
        XftColorFree(display, visual, colormap, &colors[0]);
        XftColorFree(display, visual, colormap, &colors[1]);
    }
    XFreeGC(display, gc);
}

} // namespace tiny::wm
