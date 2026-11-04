#include "style.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "display.h"
#include "text.h"

#define GET_STATE_COLOR(state, P)                                              \
    if (state & uint8_t(State::Selected) && selected_##P >> 24 != 0x01) {      \
        retval = selected_##P;                                                 \
    }                                                                          \
    if (state & uint8_t(State::Hover) && hover_##P >> 24 != 0x01) {            \
        retval = hover_##P;                                                    \
    }                                                                          \
    if (state & uint8_t(State::Focus) && focus_##P >> 24 != 0x01) {            \
        retval = focus_##P;                                                    \
    }                                                                          \
    if (state & uint8_t(State::Pressed) && pressed_##P >> 24 != 0x01) {        \
        retval = pressed_##P;                                                  \
    }                                                                          \
    if (state & uint8_t(State::Disabled)) {                                    \
        retval = disable_##P >> 24 != 0x01 ? disable_##P : normal_##P;         \
    }

#define GET_STATE_UINT32_COLOR(state, P)                                       \
    uint32_t retval = normal_##P;                                              \
    GET_STATE_COLOR(state, P);                                                 \
    return retval;

#define GET_STATE_XFT_COLOR(state, P)                                          \
    uint32_t retval = normal_##P;                                              \
    GET_STATE_COLOR(state, P);                                                 \
    char hex[10] = {'\0'};                                                     \
    snprintf(hex, 10, "#%06X", retval);                                        \
    return std::string(hex);

namespace tiny {

int scaled(int value, double factor)
{
    return static_cast<int>(lround(value * factor));
}

namespace {

//! The text with a backslash before the characters of the fontconfig syntax
std::string escaped(const std::string& text)
{
    std::string rv;
    for (const char c : text) {
        if (c == '\\' || c == '-' || c == ':' || c == ',') {
            rv += '\\';
        }
        rv += c;
    }
    return rv;
}

} // namespace

std::string xft_style(const std::string& style)
{
    return style.empty() ? "" : ":style=" + escaped(style);
}

std::string xft_name(const std::string& family, double size,
                     const std::string& style)
{
    const std::string name = escaped(family);
    char number[32];
    snprintf(number, sizeof number, "-%g", size);
    return name + number + style;
}

bool variant_from_name(const std::string& name, Variant& variant)
{
    const std::string lowered = lower(name);
    if (lowered == "dark") {
        variant = Variant::Dark;
    } else if (lowered == "light") {
        variant = Variant::Light;
    } else {
        return false;
    }
    return true;
}

const char* variant_name(Variant variant)
{
    return variant == Variant::Light ? "light" : "dark";
}

Style::~Style()
{
    if (font) {
        XftFontClose(get_display(), font);
        font = nullptr;
    }
}

void Style::colorize(Variant variant)
{
    const bool dark = variant == Variant::Dark;
    normal_bg = dark ? BLACK : 0xF6F5F4;
    normal_fg = dark ? 0xA8A8A8 : 0x2E3436;
    normal_br = dark ? 0x2E3436 : 0xD8D6D4;
    disable_fg = dark ? 0x555753 : 0x8B8E8F;
    hover_bg = dark ? 0x2E3436 : 0xDEDDDA;
    hover_fg = dark ? 0xF3F3F3 : 0x0E1416;
    selected_bg = dark ? 0x3E4446 : 0xD0DDF0;
}

void EntryStyle::colorize(Variant variant)
{
    Style::colorize(variant);
    const bool dark = variant == Variant::Dark;
    normal_bg = dark ? 0x2E3436 : 0xFFFFFF;
    normal_fg = dark ? 0xF3F3F3 : 0x2E3436;
    normal_br = dark ? 0x555753 : 0xBABDBE;
    disable_fg = 0x8B8E8F;
    focus_br = 0x729FCF;
}

void ListStyle::colorize(Variant variant)
{
    Style::colorize(variant);
    const bool dark = variant == Variant::Dark;
    selected_bg = dark ? 0x3E4446 : 0xD0DDF0;
    selected_fg = dark ? 0xF3F3F3 : 0x0E1416;
}

void Style::scale(double factor)
{
    // Remember the unscaled values on first use, so scaling is repeatable
    if (!base_set) {
        base_border_width = border_width;
        base_padding_width = padding_width;
        base_font_size = font_size;
        base_set = true;
    }

    border_width = scaled(base_border_width, factor);
    padding_width = scaled(base_padding_width, factor);
    font_size = base_font_size * factor;
}

void Style::init()
{
    const int screen = 0; // XXX: this is zero for now
    const std::string name = xft_name(font_family, font_size, font_style);
    font = XftFontOpenName(get_display(), screen, name.c_str());
    if (font == nullptr) {
        throw std::runtime_error("Failed to load font " + name);
    }
}

uint32_t Style::get_bg(uint8_t state) const {GET_STATE_UINT32_COLOR(state, bg)}

uint32_t Style::get_fg(uint8_t state) const {GET_STATE_UINT32_COLOR(state, fg)}

uint32_t Style::get_br(uint8_t state) const {GET_STATE_UINT32_COLOR(state, br)}

std::string Style::get_xft_fg(uint8_t state) const
{
    GET_STATE_XFT_COLOR(state, fg);
}

} // namespace tiny
