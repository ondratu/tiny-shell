#pragma once

#include <stdint.h>

#include "style.h"
#include "wm/styles.h"

namespace tiny {

class Theme {
  public:
    // The scale is 1 until scale() is called (the configuration sets it)
    Theme();
    ~Theme() = default;

    void init();

    // Scale all metrics and fonts (relative to the default theme)
    void scale(double factor);

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
    int wm_dock_attention_bar = 3; //!< under the icon that demands attention
    int wm_panel_app_width = 200;
    int wm_panel_login_width = 120;
    int wm_menu_width = 120;
    int wm_menu_item_height = 20;
    int wm_popup_item_height = 30; //!< an item of a menu: as high as the panel
    int wm_menu_padding = 12;      //!< left and right of the text of an item
    int wm_dock_dots = 10;         //!< under an icon, the room for its dots
    int wm_dock_dot = 4;           //!< the diameter of a dot (a window)
    int wm_dock_line = 1;          //!< the width of the line between the groups
    int wm_dock_marker = 3;        //!< the width of the mark of a drop
    int wm_dock_drag = 6;          //!< how far a press moves before it drags
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
    wm::PlainStyle wm_dock_separator{0x555753};
    wm::PlainStyle wm_dock_mark{0x729FCF};
    EntryStyle entry;
    ListStyle list;
}; // Theme

extern Theme theme;

} // namespace tiny

// default tiny-shel widgets are dark
