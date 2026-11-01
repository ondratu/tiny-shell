#pragma once

#include <stdint.h>

#include "style.h"
#include "wm/styles.h"

namespace tiny {

class Theme {
  public:
    // Scale factor is read from TINY_SCALE_FACTOR (default 1)
    Theme();
    ~Theme() = default;

    void init();

    // Scale all metrics and fonts (relative to the default theme)
    void scale(double factor);

    // Parse TINY_SCALE_FACTOR; invalid or non-positive value gives 1
    static double env_scale_factor();

    uint32_t root_background = 0x729FCF;
    int wm_win_header = 25;
    int wm_win_icon = 21; // wm_win_header - 2*header.padding
    int wm_win_border = 10;
    int wm_win_corner = wm_win_border * 2;
    int wm_win_min_width = 10;
    int wm_win_min_height = 10 + wm_win_header;

    int wm_panel = 30;
    int wm_icon = 48;
    int wm_dock_border = 5;
    int wm_panel_app_width = 200;
    int wm_panel_login_width = 120;
    int wm_menu_width = 120;
    int wm_menu_item_height = 20;
    int wm_launcher_columns = 8;
    int wm_launcher_rows = 4;
    int wm_launcher_cell_width = 64; // the narrowest cell
    int wm_launcher_label = 40;      // under an icon, for its text
    int wm_launcher_icon = 48;
    int wm_launcher_dot = 8;

    double scale_factor = 1;

    Style widget;
    wm::ButtonStyle wm_button;
    wm::HeaderStyle wm_header;
    wm::DockStyle wm_dock;
    EntryStyle entry;
    ListStyle list;
}; // Theme

extern Theme theme;

} // namespace tiny

// default tiny-shel widgets are dark
