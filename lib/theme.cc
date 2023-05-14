#include <stdio.h>

#include "display.h"
#include "theme.h"

#define GET_STATE_COLOR(state, P) \
    if (state & uint8_t(State::Selected) && selected_##P >> 24 != 0x01){ \
        retval = selected_##P; } \
    if (state & uint8_t(State::Hover) && hover_##P >> 24 != 0x01){ \
        retval = hover_##P; } \
    if (state & uint8_t(State::Focus) && focus_##P >> 24 != 0x01){ \
        retval = focus_##P; } \
    if (state & uint8_t(State::Pressed) && pressed_##P >> 24 != 0x01){ \
        retval = pressed_##P; } \
    if (state & uint8_t(State::Disabled)){ \
        retval = disable_##P >> 24 != 0x01 ? disable_##P : normal_##P; }

#define GET_STATE_UINT32_COLOR(state, P) \
    uint32_t retval = normal_##P; \
    GET_STATE_COLOR(state, P); \
    return retval;

#define GET_STATE_XFT_COLOR(state, P) \
    uint32_t retval = normal_##P; \
    GET_STATE_COLOR(state, P); \
    char hex[10] = {'\0'}; \
    snprintf(hex, 10, "#%06X", retval); \
    return std::string(hex);

namespace tiny {

Theme theme;  // default_theme

void Theme::init()  // TODO: screen, display, config...
{
    widget.init();
    wm_button.init();
    wm_header.init();
}

Style::~Style()
{
    if (font) {
        XftFontClose(get_display(), font);
        font = nullptr;
    }
}

void Style::init()
{
    const int screen = 0;   // XXX: this is zero for now
    font = XftFontOpenName(get_display(), screen, font_name.data());

    // TODO: check success
}

const uint32_t Style::get_bg(uint8_t state) const
{
    GET_STATE_UINT32_COLOR(state, bg)
}

const uint32_t Style::get_fg(uint8_t state) const
{
    GET_STATE_UINT32_COLOR(state, fg)
}

const uint32_t Style::get_br(uint8_t state) const
{
    GET_STATE_UINT32_COLOR(state, br)
}

const std::string Style::get_xft_fg(uint8_t state) const
{
    GET_STATE_XFT_COLOR(state, fg);
}

} // namespace
