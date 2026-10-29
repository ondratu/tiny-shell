#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static uint32_t scaled(uint32_t value, double factor)
{
    return uint32_t(lround(value * factor));
}

Theme::Theme()
{
    scale(env_scale_factor());
}

double Theme::env_scale_factor()
{
    const char *env = getenv("TINY_SCALE_FACTOR");
    if (!env)
        return 1;

    char *end = nullptr;
    double factor = strtod(env, &end);
    if (end == env || *end != '\0' || !(factor > 0) || factor > 16)
        return 1;
    return factor;
}

void Theme::scale(double factor)
{
    scale_factor = factor;

    wm_win_header = scaled(25, factor);
    wm_win_icon = scaled(21, factor);
    wm_win_border = scaled(10, factor);
    wm_win_corner = wm_win_border*2;
    wm_win_min_width = scaled(10, factor);
    wm_win_min_height = scaled(10, factor) + wm_win_header;

    wm_panel = scaled(30, factor);
    wm_icon = scaled(48, factor);
    wm_dock_border = scaled(5, factor);
    wm_panel_app_width = scaled(200, factor);
    wm_panel_login_width = scaled(120, factor);
    wm_menu_width = scaled(120, factor);
    wm_menu_item_height = scaled(20, factor);

    widget.scale(factor);
    wm_button.scale(factor);
    wm_header.scale(factor);
    wm_dock.scale(factor);
}

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
    const int screen = 0;   // XXX: this is zero for now
    char name[128];
    snprintf(name, sizeof(name), "%s-%g%s", font_family, font_size, font_style);
    font = XftFontOpenName(get_display(), screen, name);

    // TODO: check success
}

uint32_t Style::get_bg(uint8_t state) const
{
    GET_STATE_UINT32_COLOR(state, bg)
}

uint32_t Style::get_fg(uint8_t state) const
{
    GET_STATE_UINT32_COLOR(state, fg)
}

uint32_t Style::get_br(uint8_t state) const
{
    GET_STATE_UINT32_COLOR(state, br)
}

const std::string Style::get_xft_fg(uint8_t state) const
{
    GET_STATE_XFT_COLOR(state, fg);
}

} // namespace
