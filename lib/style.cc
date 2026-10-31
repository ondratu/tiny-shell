#include "style.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "display.h"

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

Style::~Style()
{
    if (font) {
        XftFontClose(get_display(), font);
        font = nullptr;
    }
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
    char name[128];
    snprintf(name, sizeof(name), "%s-%g%s", font_family, font_size, font_style);
    font = XftFontOpenName(get_display(), screen, name);
    if (font == nullptr) {
        throw std::runtime_error(std::string("Failed to load font ") + name);
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
