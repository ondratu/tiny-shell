#include "plugin/widgets.h"

#include <fontconfig/fontconfig.h>

#include <X11/keysym.h>

#include <algorithm>

#include "display.h"
#include "wm/shell_theme.h"

namespace tiny::plugin {

namespace {

int extent(::Display* display, XftFont* font, const std::string& text)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

//! A color for the text from the style, to draw with Xft
struct Ink {
    Ink(::Display* display_arg, uint32_t rgb): display(display_arg)
    {
        const int screen = XDefaultScreen(display);
        visual = XDefaultVisual(display, screen);
        colormap = XDefaultColormap(display, screen);
        XRenderColor wanted;
        wanted.red = static_cast<unsigned short>(((rgb >> 16) & 0xFFU) * 257);
        wanted.green = static_cast<unsigned short>(((rgb >> 8) & 0xFFU) * 257);
        wanted.blue = static_cast<unsigned short>((rgb & 0xFFU) * 257);
        wanted.alpha = 0xFFFF;
        XftColorAllocValue(display, visual, colormap, &wanted, &color);
    }

    Ink(const Ink&) = delete;
    Ink& operator=(const Ink&) = delete;

    ~Ink() { XftColorFree(display, visual, colormap, &color); }

    ::Display* display;
    Visual* visual = nullptr;
    Colormap colormap = 0;
    XftColor color;
};

void text_at(XftDraw* draw, const Ink& ink, XftFont* font, int x, int top,
             int height, const std::string& text)
{
    XftDrawStringUtf8(draw, &ink.color, font, x,
                      top + (height + font->ascent - font->descent) / 2,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));
}

//! A filled rectangle (Xft wants unsigned sizes)
void fill_rect(XftDraw* draw, const XftColor& color, int x, int y, int w, int h)
{
    XftDrawRect(draw, &color, x, y, static_cast<unsigned>(std::max(w, 0)),
                static_cast<unsigned>(std::max(h, 0)));
}

//! The field of a row: a frame and the text in it, with the cursor when the
//! field has the keys. The end of the text is what is shown when it is longer
//! than the field.
void paint_field(::Display* display, XftDraw* draw, const Ink& ink,
                 const Ink& faint, XftFont* font, int field_width, int right,
                 int top, int height, bool focus, std::string text,
                 size_t cursor)
{
    const int fx = right - field_width;
    const int fy = top + 1;
    const int fh = height - 2;
    for (const Rect& edge :
         {Rect{fx, fy, field_width, 1}, Rect{fx, fy + fh - 1, field_width, 1},
          Rect{fx, fy, 1, fh}, Rect{fx + field_width - 1, fy, 1, fh}}) {
        fill_rect(draw, faint.color, edge.x, edge.y, edge.width, edge.height);
    }
    while (!text.empty() && extent(display, font, text) > field_width - 8) {
        size_t drop = 1;
        while (drop < text.size() &&
               (static_cast<unsigned char>(text[drop]) & 0xC0U) == 0x80U) {
            ++drop;
        }
        text.erase(0, drop);
        cursor = cursor > drop ? cursor - drop : 0;
    }
    text_at(draw, ink, font, fx + 4, fy, fh, text);
    if (focus) {
        const int x = fx + 4 + extent(display, font, text.substr(0, cursor));
        fill_rect(draw, ink.color, x, fy + 3, 1, fh - 6);
    }
}

XftFont* scaled_font(::Display* display, const XftFont* base, double factor)
{
    FcPattern* pattern = FcPatternDuplicate(base->pattern);
    double pixels = 0;
    if (pattern == nullptr || FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0,
                                                 &pixels) != FcResultMatch) {
        if (pattern != nullptr) {
            FcPatternDestroy(pattern);
        }
        return nullptr;
    }
    FcPatternDel(pattern, FC_PIXEL_SIZE);
    FcPatternDel(pattern, FC_SIZE);
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixels * factor);
    return XftFontOpenPattern(display, pattern); // takes the pattern
}

} // namespace

PanelFont::~PanelFont()
{
    close();
}

void PanelFont::close()
{
    // the font of a display that is closed is closed with it
    if (small != nullptr && Display::live_serial() == serial) {
        XftFontClose(opened_on, small);
    }
    small = nullptr;
}

XftFont* PanelFont::get(::Display* display, XftFont* from)
{
    if (from == nullptr) {
        return nullptr;
    }
    if (from != base || display != opened_on ||
        Display::live_serial() != serial) {
        close();
        base = from;
        opened_on = display;
        serial = Display::live_serial();
        small = scaled_font(display, from, factor);
    }
    return small != nullptr ? small : from;
}

int panel_text_width(::Display* display, int padding, XftFont* font,
                     const std::string& text, const std::string& sample)
{
    return std::max(extent(display, font, text),
                    extent(display, font, sample)) +
           2 * padding;
}

int ItemIcon::size_for(int height)
{
    return height * 3 / 5;
}

int ItemIcon::width_for(int height) const
{
    if (!room) {
        return 0;
    }
    const int size = size_for(height);
    return size + std::max(3, size / 4);
}

uint32_t mixed(uint32_t base, uint32_t towards, int percent)
{
    uint32_t out = 0;
    for (const unsigned shift : {16U, 8U, 0U}) {
        const auto a = static_cast<int>((base >> shift) & 0xFFU);
        const auto b = static_cast<int>((towards >> shift) & 0xFFU);
        out |= static_cast<uint32_t>((a * (100 - percent) + b * percent + 50) /
                                     100)
               << shift;
    }
    return out;
}

uint32_t dimmed(uint32_t foreground, uint32_t background)
{
    uint32_t out = 0;
    for (const unsigned shift : {16U, 8U, 0U}) {
        const uint32_t a = (foreground >> shift) & 0xFFU;
        const uint32_t b = (background >> shift) & 0xFFU;
        out |= ((a + b + 1) / 2) << shift;
    }
    return out;
}

void paint_item(::Display* display, ::Window window, const Style& style,
                uint8_t state, int width, int height, XftFont* font,
                const std::string& text, const ItemIcon& icon, int padding,
                bool center)
{
    const uint32_t fg = style.get_fg(state);
    const uint32_t bg = style.get_bg(state);
    XSetWindowBackground(display, window, bg);
    XClearWindow(display, window);

    int x = padding;
    if (icon.room) {
        const int size = ItemIcon::size_for(height);
        if (icon.icons != nullptr) {
            if (!icon.under.empty()) {
                icon.icons->draw(window, icon.under, size, dimmed(fg, bg), size,
                                 height, x, 0);
            }
            if (!icon.name.empty()) {
                icon.icons->draw(window, icon.name, size, fg, size, height, x,
                                 0);
            }
        }
        x += icon.width_for(height);
    }
    const Ink ink(display, fg);
    const int screen = XDefaultScreen(display);
    XftDraw* draw =
            XftDrawCreate(display, window, XDefaultVisual(display, screen),
                          XDefaultColormap(display, screen));
    if (center && !icon.room) {
        x = std::max(padding, (width - extent(display, font, text)) / 2);
    }
    text_at(draw, ink, font, x, 0, height, text);
    XftDrawDestroy(draw);
}

PluginLabel::PluginLabel(const std::string& initial, const std::string& sample,
                         ItemIcon icon):
    tiny::Label(1, wm::shell_theme.panel, initial), item(std::move(icon))
{
    name = "plugin_label";
    resize(panel_text_width(display, get_padding(),
                            font.get(display, get_style().get_font()), initial,
                            sample) +
                   item.width_for(wm::shell_theme.panel),
           wm::shell_theme.panel);
}

PluginLabel::~PluginLabel() = default;

void PluginLabel::set_icon(const std::string& icon, const std::string& under)
{
    item.name = icon;
    item.under = under;
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void PluginLabel::on_expose(const XEvent&, void*)
{
    paint_item(display, window, get_style(), get_theme_state(), width, height,
               font.get(display, get_style().get_font()), get_text(), item,
               get_padding(), false);
}

PluginButton::PluginButton(const std::string& initial,
                           const std::string& sample, ItemIcon icon):
    tiny::LabelButton(1, wm::shell_theme.panel, initial), item(std::move(icon))
{
    name = "plugin_button";
    set_border(0);
    resize(panel_text_width(display, get_padding(),
                            font.get(display, get_style().get_font()), initial,
                            sample) +
                   item.width_for(wm::shell_theme.panel),
           wm::shell_theme.panel);
}

PluginButton::~PluginButton() = default;

void PluginButton::set_icon(const std::string& icon, const std::string& under)
{
    item.name = icon;
    item.under = under;
    if (is_maped) {
        on_expose(XEvent(), nullptr);
    }
}

void PluginButton::on_expose(const XEvent&, void*)
{
    paint_item(display, window, get_style(), get_theme_state(), width, height,
               font.get(display, get_style().get_font()), get_text(), item,
               get_padding(), true);
}

// ---- the window of a plugin ----

PopupLayout
popup_layout(const PopupContent& content, int line, int icon_size, int big_line,
             int limit,
             const std::function<int(const std::string&)>& text_width,
             const std::function<int(const std::string&)>& big_width)
{
    const int pad = std::max(10, line * 4 / 5);
    const int gap = pad / 2;
    PopupLayout out;
    out.field_width = text_width("mmmmmmmmmmmmmmmm");
    int wide = 240;
    int y = pad;

    const bool has_header = !content.icon.empty() || !content.title.empty() ||
                            !content.subtitle.empty() || !content.value.empty();
    if (has_header) {
        const int icon = content.icon.empty() ? 0 : 2 * icon_size;
        out.header_height = std::max(
                {icon, 2 * line + 2, content.value.empty() ? 0 : big_line});
        int need = 2 * pad + std::max(text_width(content.title),
                                      text_width(content.subtitle));
        need += icon != 0 ? icon + pad : 0;
        need += content.value.empty() ? 0 : big_width(content.value) + pad;
        wide = std::max(wide, need);
        y += out.header_height + pad;
    }
    for (const PopupSection& section : content.sections) {
        PopupLayout::Line heading;
        if (!section.heading.empty()) {
            heading.y = y;
            heading.height = line + gap;
            y += heading.height;
            wide = std::max(wide, 2 * pad + text_width(section.heading));
        }
        out.headings.push_back(heading);
        std::vector<PopupLayout::Line> rows;
        const bool icons =
                std::any_of(section.rows.begin(), section.rows.end(),
                            [](const PopupRow& r) { return !r.icon.empty(); });
        for (const PopupRow& row : section.rows) {
            PopupLayout::Line one;
            one.y = y;
            one.height = std::max(line, icon_size) + gap;
            y += one.height;
            rows.push_back(one);
            const int badge = row.badge.empty() ? 0 : icon_size + gap;
            const int value =
                    row.entry ? out.field_width : text_width(row.value) + badge;
            wide = std::max(wide, 2 * pad + (icons ? icon_size + gap : 0) +
                                          text_width(row.label) + 2 * pad +
                                          value);
        }
        out.rows.push_back(std::move(rows));
        y += gap;
    }
    out.width = std::min(wide, std::max(limit, 1));
    out.height = std::max(y, 2 * pad);
    return out;
}

std::vector<RowRef> popup_targets(const PopupContent& content)
{
    std::vector<RowRef> found;
    for (size_t s = 0; s < content.sections.size(); ++s) {
        const auto& rows = content.sections[s].rows;
        for (size_t r = 0; r < rows.size(); ++r) {
            if (rows[r].target()) {
                found.push_back({static_cast<int>(s), static_cast<int>(r)});
            }
        }
    }
    return found;
}

RowRef popup_row_at(const PopupLayout& layout, const PopupContent& content,
                    int y)
{
    for (const RowRef& row : popup_targets(content)) {
        const auto s = static_cast<size_t>(row.section);
        const auto r = static_cast<size_t>(row.row);
        if (s >= layout.rows.size() || r >= layout.rows[s].size()) {
            continue;
        }
        const PopupLayout::Line& line = layout.rows[s][r];
        if (y >= line.y && y < line.y + line.height) {
            return row;
        }
    }
    return {};
}

int clamp_scroll(int scroll, int content_height, int view_height)
{
    return std::max(0, std::min(scroll, content_height - view_height));
}

int scroll_to_show(int scroll, int top, int bottom, int view_height)
{
    if (top < scroll || bottom - top >= view_height) {
        return top;
    }
    if (bottom > scroll + view_height) {
        return bottom - view_height;
    }
    return scroll;
}

namespace {

//! The number of characters (not bytes) of a UTF-8 text
size_t characters(const std::string& text)
{
    return static_cast<size_t>(
            std::count_if(text.begin(), text.end(), [](char c) {
                return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U;
            }));
}

//! The longest text that a field takes, in bytes
constexpr size_t field_limit = 256;

} // namespace

PluginPopup::PluginPopup(wm::SymbolicIcons* icons_arg):
    tiny::Popover(1, 1), icons(icons_arg)
{
    name = "plugin_popup";
    set_style_override(&wm::shell_theme.panel_widgets);
    field.reserve(field_limit);
}

PluginPopup::~PluginPopup()
{
    if (event_done) {
        disconnect(Expose);
        disconnect(KeyPress);
        disconnect(ButtonPress);
        disconnect(MotionNotify);
    }
    field.wipe();
    close_font();
}

void PluginPopup::close_font()
{
    // the font of a display that is closed is closed with it
    if (big_font != nullptr && Display::live_serial() == big_font_serial) {
        XftFontClose(display, big_font);
    }
    big_font = nullptr;
}

void PluginPopup::realize(::Window root, int x, int y)
{
    tiny::Popover::realize(root, x, y);
    input = std::make_unique<InputMethod>(display, window);
}

void PluginPopup::set_events(long mask)
{
    tiny::Popover::set_events(mask | ExposureMask | KeyPressMask |
                              ButtonPressMask | PointerMotionMask);
    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
    connect(KeyPress, [this](const XEvent& e) { on_key(e); });
    connect(ButtonPress, [this](const XEvent& e) { on_press(e); });
    connect(MotionNotify, [this](const XEvent& e) { on_motion(e); });
}

void PluginPopup::unmap()
{
    field.wipe(); // a password is not kept for the next time
    field_key.clear();
    tiny::Popover::unmap();
}

const PopupRow* PluginPopup::row_of(RowRef row) const
{
    if (!row.valid() ||
        static_cast<size_t>(row.section) >= shown.sections.size()) {
        return nullptr;
    }
    const auto& rows = shown.sections[static_cast<size_t>(row.section)].rows;
    return static_cast<size_t>(row.row) < rows.size()
                   ? &rows[static_cast<size_t>(row.row)]
                   : nullptr;
}

void PluginPopup::open(const PopupContent& content, ::Window anchor,
                       ::Window bounds)
{
    XftFont* font = get_style().get_font();
    close_font(); // the font of the style may have changed since the last time
    big_font = scaled_font(display, font, 2.2);
    big_font_serial = Display::live_serial();
    XftFont* big = big_font != nullptr ? big_font : font;

    XWindowAttributes at;
    XWindowAttributes inside;
    XWindowAttributes screen;
    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    ::Window ignored = None;
    if (XGetWindowAttributes(display, anchor, &at) == 0 ||
        XGetWindowAttributes(display, bounds, &inside) == 0 ||
        XGetWindowAttributes(display, parent, &screen) == 0 ||
        XTranslateCoordinates(display, anchor, parent, 0, 0, &ax, &ay,
                              &ignored) == 0 ||
        XTranslateCoordinates(display, bounds, parent, 0, 0, &bx, &by,
                              &ignored) == 0) {
        return;
    }

    // What stays when a window that is open is shown again (an answer that
    // came late): the lit row, the text typed in the field and the place
    const bool again = is_maped;
    std::string lit;
    if (again) {
        if (const PopupRow* row = row_of(selected)) {
            lit = row->key();
        }
    } else {
        field.wipe();
        field_key.clear();
        scroll = 0;
    }
    shown = content;
    selected = {};
    std::string entry_key;
    for (const RowRef& row : popup_targets(shown)) {
        const PopupRow* const one = row_of(row);
        if (one->entry && entry_key.empty()) {
            entry_key = one->key();
            selected = row; // the user goes to type
        }
    }
    // a field that was not there before gets the keys, whatever was lit
    const bool new_field = !entry_key.empty() && entry_key != field_key;
    if (entry_key != field_key) {
        field.wipe();
        field_key = entry_key;
    }
    if (again && !lit.empty() && !new_field) {
        for (const RowRef& row : popup_targets(shown)) {
            if (row_of(row)->key() == lit) {
                selected = row;
            }
        }
    }

    line_height = font->ascent + font->descent;
    layout = popup_layout(
            shown, line_height, ItemIcon::size_for(wm::shell_theme.panel),
            big->ascent + big->descent, inside.width - 2 * get_border(),
            [&](const std::string& s) { return extent(display, font, s); },
            [&](const std::string& s) { return extent(display, big, s); });
    // a window that does not fit under the widget is scrolled
    const int pad = std::max(10, line_height * 4 / 5);
    const int room = screen.height - (ay + at.height) - 2 * get_border() - pad;
    view_height = std::min(layout.height, std::max(room, 4 * line_height));
    scroll = clamp_scroll(scroll, layout.height, view_height);
    resize(layout.width, view_height);
    // the server tells the size later, popup() needs it now
    width = layout.width;
    height = view_height;
    if (!again) {
        reveal_selected();
    }
    // under the widget, but inside of the panel
    int left = ax + at.width / 2 - layout.width / 2;
    left = std::max(bx, std::min(left, bx + inside.width - layout.width -
                                               2 * get_border()));
    popup(left + layout.width / 2, ay + at.height);
    on_expose(XEvent(), nullptr);
}

std::string PluginPopup::field_text(size_t* cursor) const
{
    const std::string& text = field.text();
    const bool secret = std::any_of(
            shown.sections.begin(), shown.sections.end(),
            [](const PopupSection& section) {
                return std::any_of(
                        section.rows.begin(), section.rows.end(),
                        [](const PopupRow& r) { return r.entry && r.secret; });
            });
    if (!secret) {
        *cursor = field.cursor();
        return text;
    }
    std::string dots;
    for (size_t i = 0; i < characters(text); ++i) {
        dots += "•";
    }
    *cursor = characters(text.substr(0, field.cursor())) * 3;
    return dots;
}

void PluginPopup::on_expose(const XEvent&, void*)
{
    paint(0, view_height);
}

//! Only what is between `from` and `to` (pixels of the window) is cleared and
//! drawn again, so a change of the lit row does not make the window flash
void PluginPopup::paint(int from, int to)
{
    if (!is_maped) {
        return;
    }
    const bool whole = from <= 0 && to >= view_height;
    const uint8_t state = get_theme_state();
    const Style& style = get_style();
    const uint32_t fg = style.get_fg(state);
    const uint32_t bg = style.get_bg(state);
    const uint32_t soft = dimmed(fg, bg);
    XftFont* font = style.get_font();
    XftFont* big = big_font != nullptr ? big_font : font;
    const int line = font->ascent + font->descent;
    const int icon_size = ItemIcon::size_for(wm::shell_theme.panel);
    const int pad = std::max(10, line * 4 / 5);
    const int gap = pad / 2;
    const int right = layout.width - pad;
    const int off = -scroll; // the content is moved up by what is scrolled

    XSetWindowBackground(display, window, bg);
    if (whole) {
        XClearWindow(display, window);
    } else {
        XClearArea(display, window, 0, from,
                   static_cast<unsigned>(layout.width),
                   static_cast<unsigned>(std::max(to - from, 0)), False);
    }
    const int screen = XDefaultScreen(display);
    XftDraw* draw =
            XftDrawCreate(display, window, XDefaultVisual(display, screen),
                          XDefaultColormap(display, screen));
    const XRectangle clip = {
            0, static_cast<short>(from),
            static_cast<unsigned short>(layout.width),
            static_cast<unsigned short>(std::max(to - from, 0))};
    XftDrawSetClipRectangles(draw, 0, 0, &clip, 1);
    const Ink ink(display, fg);
    const Ink faint(display, soft);
    const Ink lit(display, mixed(bg, fg, 16));

    if (whole && layout.header_height > 0) {
        int x = pad;
        const int top = pad + off;
        if (!shown.icon.empty() && icons != nullptr) {
            const int size = 2 * icon_size;
            icons->draw(window, shown.icon, size, fg, size,
                        layout.header_height, x, top);
            x += size + pad;
        }
        const int lines = shown.subtitle.empty() ? 1 : 2;
        const int block = lines * line + (lines - 1) * 2;
        const int first = top + (layout.header_height - block) / 2;
        text_at(draw, ink, font, x, first, line, shown.title);
        text_at(draw, faint, font, x, first + line + 2, line, shown.subtitle);
        if (!shown.value.empty()) {
            const int w = extent(display, big, shown.value);
            text_at(draw, ink, big, right - w, top, layout.header_height,
                    shown.value);
        }
    }
    for (size_t i = 0; i < shown.sections.size(); ++i) {
        const PopupSection& section = shown.sections[i];
        if (whole && !section.heading.empty()) {
            text_at(draw, faint, font, pad, layout.headings[i].y + off,
                    layout.headings[i].height - gap / 2, section.heading);
        }
        const bool icons_here =
                std::any_of(section.rows.begin(), section.rows.end(),
                            [](const PopupRow& r) { return !r.icon.empty(); });
        for (size_t r = 0; r < section.rows.size(); ++r) {
            const PopupRow& row = section.rows[r];
            const PopupLayout::Line& at = layout.rows[i][r];
            const int top = at.y + off;
            if (top + at.height <= from || top >= to || top + at.height < 0 ||
                top > view_height) {
                continue; // scrolled away, or not what is drawn again
            }
            const bool on = selected ==
                            RowRef{static_cast<int>(i), static_cast<int>(r)};
            if (on) {
                fill_rect(draw, lit.color, 0, top, layout.width, at.height);
            }
            const Ink& main = row.dim ? faint : ink;
            int x = pad;
            if (icons_here) {
                if (!row.icon.empty() && icons != nullptr) {
                    icons->draw(window, row.icon, icon_size,
                                row.dim ? soft : fg, icon_size, at.height, x,
                                top);
                }
                x += icon_size + gap;
            }
            text_at(draw, main, font, x, top, at.height, row.label);
            if (row.entry) {
                size_t cursor = 0;
                const std::string typed = field_text(&cursor);
                paint_field(display, draw, ink, faint, font, layout.field_width,
                            right, top, at.height, on, typed, cursor);
                continue;
            }
            const int w = extent(display, font, row.value);
            text_at(draw, faint, font, right - w, top, at.height, row.value);
            if (!row.badge.empty() && icons != nullptr) {
                icons->draw(window, row.badge, icon_size, soft, icon_size,
                            at.height, right - w - gap - icon_size, top);
            }
        }
    }
    if (whole &&
        layout.height > view_height) { // where the window is in the content
        const int bar =
                std::max(2 * pad, view_height * view_height / layout.height);
        const int at = (view_height - bar) * scroll /
                       std::max(1, layout.height - view_height);
        fill_rect(draw, faint.color, layout.width - 3, at, 3, bar);
    }
    XftDrawDestroy(draw);
}

void PluginPopup::select(RowRef row)
{
    if (row == selected) {
        return;
    }
    const RowRef before = selected;
    selected = row;
    repaint_rows(before, row);
}

//! Draws two rows again (the one that is not lit now and the one that is)
void PluginPopup::repaint_rows(RowRef a, RowRef b)
{
    for (const RowRef& row : {a, b}) {
        if (!row.valid()) {
            continue;
        }
        const PopupLayout::Line& at =
                layout.rows[static_cast<size_t>(row.section)]
                           [static_cast<size_t>(row.row)];
        paint(at.y - scroll, at.y - scroll + at.height);
    }
}

void PluginPopup::reveal_selected()
{
    if (!selected.valid()) {
        return;
    }
    const PopupLayout::Line& at =
            layout.rows[static_cast<size_t>(selected.section)]
                       [static_cast<size_t>(selected.row)];
    const std::vector<RowRef> targets = popup_targets(shown);
    // the first and the last of them show what is before and after them
    const int top = selected == targets.front() ? 0 : at.y;
    const int bottom =
            selected == targets.back() ? layout.height : at.y + at.height;
    scroll = clamp_scroll(scroll_to_show(scroll, top, bottom, view_height),
                          layout.height, view_height);
}

void PluginPopup::move_selection(int step)
{
    const std::vector<RowRef> targets = popup_targets(shown);
    if (targets.empty()) {
        return;
    }
    const auto count = static_cast<int>(targets.size());
    const auto found = std::find(targets.begin(), targets.end(), selected);
    int index = step > 0 ? 0 : count - 1;
    if (found != targets.end()) {
        index = std::max(
                0, std::min(count - 1,
                            static_cast<int>(found - targets.begin()) + step));
    }
    const RowRef before = selected;
    const int old_scroll = scroll;
    selected = targets[static_cast<size_t>(index)];
    reveal_selected();
    if (scroll != old_scroll) {
        on_expose(XEvent(), nullptr);
    } else {
        repaint_rows(before, selected);
    }
}

void PluginPopup::scroll_by(int delta)
{
    const int now = scroll;
    scroll = clamp_scroll(scroll + delta, layout.height, view_height);
    if (scroll != now) {
        on_expose(XEvent(), nullptr);
    }
}

void PluginPopup::activate(RowRef row)
{
    const PopupRow* const one = row_of(row);
    if (one == nullptr || !one->target()) {
        return;
    }
    // what is needed is copied: the plugin may show the window again from the
    // action, which replaces the rows
    const int number = one->action;
    if (number < 0) { // a field without a function keeps what was typed
        return;
    }
    std::string text;
    if (one->entry) {
        text = field.text();
        field.wipe();
    }
    if (action) {
        action(number, text);
    }
    std::fill(text.begin(), text.end(), '\0'); // not left in memory
}

void PluginPopup::on_press(const XEvent& e)
{
    switch (e.xbutton.button) {
        case Button4: // wheel up
            scroll_by(-3 * line_height);
            break;
        case Button5: // wheel down
            scroll_by(3 * line_height);
            break;
        case Button1: {
            const RowRef row =
                    popup_row_at(layout, shown, e.xbutton.y + scroll);
            if (row.valid()) {
                select(row);
                if (!row_of(row)->entry) {
                    activate(row);
                }
            }
            break;
        }
        default:
            break;
    }
}

void PluginPopup::on_motion(const XEvent& e)
{
    // a hand that brushes the mouse must not take the keys from the field
    // while a password is typed: only a click or Up and Down leave it
    const PopupRow* const lit = row_of(selected);
    if (lit != nullptr && lit->entry) {
        return;
    }
    const RowRef row = popup_row_at(layout, shown, e.xmotion.y + scroll);
    if (row.valid()) {
        select(row);
    }
}

void PluginPopup::on_key(const XEvent& e)
{
    KeySym keysym = NoSymbol;
    std::string typed;
    if (input != nullptr) {
        typed = input->lookup(e.xkey, keysym);
    } else {
        keysym = XLookupKeysym(const_cast<XKeyEvent*>(&e.xkey), 0);
    }
    const PopupRow* const row = row_of(selected);
    const bool in_field = row != nullptr && row->entry;
    bool changed = true;
    switch (keysym) {
        case XK_Escape:
            unmap();
            return;
        case XK_Up:
            move_selection(-1);
            return;
        case XK_Down:
        case XK_Tab:
            move_selection(1);
            return;
        case XK_ISO_Left_Tab:
            move_selection(-1);
            return;
        case XK_Return:
        case XK_KP_Enter:
            activate(selected);
            return;
        case XK_Prior:
            scroll_by(-view_height);
            return;
        case XK_Next:
            scroll_by(view_height);
            return;
        case XK_BackSpace:
            changed = in_field && field.backspace();
            break;
        case XK_Delete:
            changed = in_field && field.erase();
            break;
        case XK_Left:
            changed = in_field && field.left();
            break;
        case XK_Right:
            changed = in_field && field.right();
            break;
        case XK_Home:
            changed = in_field && field.home();
            break;
        case XK_End:
            changed = in_field && field.end();
            break;
        default: {
            const bool command = (e.xkey.state & (ControlMask | Mod1Mask)) != 0;
            const bool printable =
                    !typed.empty() && !command &&
                    static_cast<unsigned char>(typed[0]) >= 0x20 &&
                    typed[0] != 0x7F;
            changed = in_field && printable &&
                      field.text().size() + typed.size() <= field_limit &&
                      field.insert(typed);
            break;
        }
    }
    if (changed) {
        on_expose(XEvent(), nullptr);
    }
}

} // namespace tiny::plugin
