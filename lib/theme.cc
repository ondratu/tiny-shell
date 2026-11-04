#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "display.h"
#include "theme.h"

namespace tiny {

Theme theme; // default_theme

Theme::Theme()
{
    scale(1);
}

void Theme::scale(double factor)
{
    scale_factor = factor;

    wm_win_header = scaled(25, factor);
    wm_win_icon = scaled(21, factor);
    wm_win_border = scaled(10, factor);
    wm_win_corner = wm_win_border * 2;
    wm_win_min_width = scaled(10, factor);
    wm_win_min_height = scaled(10, factor) + wm_win_header;

    wm_panel = scaled(30, factor);
    wm_icon = scaled(48, factor);
    wm_dock_border = scaled(5, factor);
    wm_dock_attention_bar = scaled(3, factor);
    wm_panel_app_width = scaled(200, factor);
    wm_panel_login_width = scaled(120, factor);
    wm_menu_width = scaled(120, factor);
    wm_menu_item_height = scaled(20, factor);
    wm_popup_item_height = scaled(30, factor);
    wm_menu_padding = scaled(12, factor);
    wm_dock_dot = scaled(4, factor);
    wm_dock_line = scaled(1, factor);
    wm_dock_marker = scaled(3, factor);
    wm_dock_drag = scaled(6, factor);
    wm_launcher_cell_width = scaled(64, factor);
    wm_launcher_label = scaled(40, factor);
    wm_launcher_icon = scaled(48, factor);
    wm_launcher_dot = scaled(8, factor);

    widget.scale(factor);
    wm_button.scale(factor);
    wm_header.scale(factor);
    wm_dock.scale(factor);
    entry.scale(factor);
    list.scale(factor);
}

void Theme::init() // TODO: screen, display, config...
{
    widget.init();
    wm_button.init();
    wm_header.init();
    entry.init();
    list.init();
}

} // namespace tiny
