#include "plugin/widgets.h"

#include <fontconfig/fontconfig.h>

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

void text_at(XftDraw* draw, const Ink& ink, XftFont* font,
             int x, int top, int height, const std::string& text)
{
    XftDrawStringUtf8(draw, &ink.color, font, x,
                      top + (height + font->ascent - font->descent) / 2,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));
}

XftFont* scaled_font(::Display* display, const XftFont* base, double factor)
{
    FcPattern* pattern = FcPatternDuplicate(base->pattern);
    double pixels = 0;
    if (pattern == nullptr ||
        FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &pixels) !=
                FcResultMatch) {
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
                uint8_t state, int height, XftFont* font,
                const std::string& text, const ItemIcon& icon, int padding)
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
    XftDraw* draw = XftDrawCreate(display, window, XDefaultVisual(display, screen),
                                  XDefaultColormap(display, screen));
    text_at(draw, ink, font, x, 0, height, text);
    XftDrawDestroy(draw);
}

PluginLabel::PluginLabel(const std::string& initial, const std::string& sample,
                         ItemIcon icon):
    tiny::Label(1, wm::shell_theme.panel, initial), item(std::move(icon))
{
    name = "plugin_label";
    resize(panel_text_width(display, get_padding(), get_style().get_font(),
                            initial, sample) +
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
    paint_item(display, window, get_style(), get_theme_state(), height,
               get_style().get_font(), get_text(), item, get_padding());
}

PluginButton::PluginButton(const std::string& initial,
                           const std::string& sample, ItemIcon icon):
    tiny::LabelButton(1, wm::shell_theme.panel, initial), item(std::move(icon))
{
    name = "plugin_button";
    set_border(0);
    resize(panel_text_width(display, get_padding(), get_style().get_font(),
                            initial, sample) +
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
    paint_item(display, window, get_style(), get_theme_state(), height,
               get_style().get_font(), get_text(), item, get_padding());
}

// ---- the window of a plugin ----

PopupLayout popup_layout(
        const PopupContent& content, int line, int icon_size, int big_line,
        int limit, const std::function<int(const std::string&)>& text_width,
        const std::function<int(const std::string&)>& big_width)
{
    const int pad = std::max(10, line * 4 / 5);
    const int gap = pad / 2;
    PopupLayout out;
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
            wide = std::max(wide, 2 * pad + (icons ? icon_size + gap : 0) +
                                          text_width(row.label) + 2 * pad +
                                          text_width(row.value));
        }
        out.rows.push_back(std::move(rows));
        y += gap;
    }
    out.width = std::min(wide, std::max(limit, 1));
    out.height = std::max(y, 2 * pad);
    return out;
}

PluginPopup::PluginPopup(wm::SymbolicIcons* icons_arg):
    tiny::Popover(1, 1), icons(icons_arg)
{
    name = "plugin_popup";
    set_style_override(&wm::shell_theme.panel_widgets);
}

PluginPopup::~PluginPopup()
{
    if (event_done) {
        disconnect(Expose);
    }
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

void PluginPopup::set_events(long mask)
{
    tiny::Popover::set_events(mask | ExposureMask);
    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
}

void PluginPopup::open(const PopupContent& content, ::Window anchor,
                       ::Window bounds)
{
    shown = content;
    XftFont* font = get_style().get_font();
    if (big_font == nullptr) {
        big_font = scaled_font(display, font, 2.2);
        big_font_serial = Display::live_serial();
    }
    XftFont* big = big_font != nullptr ? big_font : font;

    XWindowAttributes at;
    XWindowAttributes inside;
    int ax = 0;
    int ay = 0;
    int bx = 0;
    int by = 0;
    ::Window ignored = None;
    if (XGetWindowAttributes(display, anchor, &at) == 0 ||
        XGetWindowAttributes(display, bounds, &inside) == 0 ||
        XTranslateCoordinates(display, anchor, parent, 0, 0, &ax, &ay,
                              &ignored) == 0 ||
        XTranslateCoordinates(display, bounds, parent, 0, 0, &bx, &by,
                              &ignored) == 0) {
        return;
    }
    layout = popup_layout(
            shown, font->ascent + font->descent,
            ItemIcon::size_for(wm::shell_theme.panel), big->ascent + big->descent,
            inside.width - 2 * get_border(),
            [&](const std::string& s) { return extent(display, font, s); },
            [&](const std::string& s) { return extent(display, big, s); });
    resize(layout.width, layout.height);
    // under the widget, but inside of the panel
    int left = ax + at.width / 2 - layout.width / 2;
    left = std::max(bx, std::min(left, bx + inside.width - layout.width -
                                               2 * get_border()));
    popup(left + layout.width / 2, ay + at.height);
    on_expose(XEvent(), nullptr);
}

void PluginPopup::on_expose(const XEvent&, void*)
{
    if (!is_maped) {
        return;
    }
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

    XSetWindowBackground(display, window, bg);
    XClearWindow(display, window);
    const int screen = XDefaultScreen(display);
    XftDraw* draw = XftDrawCreate(display, window, XDefaultVisual(display, screen),
                                  XDefaultColormap(display, screen));
    const Ink ink(display, fg);
    const Ink faint(display, soft);

    if (layout.header_height > 0) {
        int x = pad;
        const int top = pad;
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
        text_at(draw, faint, font, x, first + line + 2, line,
                shown.subtitle);
        if (!shown.value.empty()) {
            const int w = extent(display, big, shown.value);
            text_at(draw, ink, big, right - w, top,
                    layout.header_height, shown.value);
        }
    }
    for (size_t i = 0; i < shown.sections.size(); ++i) {
        const PopupSection& section = shown.sections[i];
        if (!section.heading.empty()) {
            text_at(draw, faint, font, pad, layout.headings[i].y,
                    layout.headings[i].height - gap / 2, section.heading);
        }
        const bool icons_here =
                std::any_of(section.rows.begin(), section.rows.end(),
                            [](const PopupRow& r) { return !r.icon.empty(); });
        for (size_t r = 0; r < section.rows.size(); ++r) {
            const PopupRow& row = section.rows[r];
            const PopupLayout::Line& at = layout.rows[i][r];
            int x = pad;
            if (icons_here) {
                if (!row.icon.empty() && icons != nullptr) {
                    icons->draw(window, row.icon, icon_size, fg, icon_size,
                                at.height, x, at.y);
                }
                x += icon_size + gap;
            }
            text_at(draw, ink, font, x, at.y, at.height, row.label);
            const int w = extent(display, font, row.value);
            text_at(draw, faint, font, right - w, at.y, at.height,
                    row.value);
        }
    }
    XftDrawDestroy(draw);
}

} // namespace tiny::plugin
