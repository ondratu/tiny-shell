#pragma once

#include <stdint.h>

#include <string>

#include "style.h"
#include "theme.h"
#include "wm/styles.h"

namespace tiny::wm {

/**
 * @short The metrics and the styles of the window manager and the shell.
 *
 * It sets the theme of the toolkit (`tiny::theme`) together with its own
 * parts, so a program of the shell has one place to configure: the font, the
 * scale and the colors of the two variants.
 */
class ShellTheme {
  public:
    //! `toolkit` is the theme of the widgets that this one sets together with
    //! its own parts. The scale is 1 until scale() is called (the
    //! configuration sets it).
    explicit ShellTheme(Theme& toolkit = tiny::theme);

    //! Open the fonts of all the styles, the toolkit ones too
    void init();

    //! The family of the font of all the styles; call before `init()`
    void set_font(const std::string& family);

    //! The size in points at the scale 1; call before `scale()` and `init()`
    void set_font_size(double size);

    //! The style of the font (e.g. "Italic") of the styles but the header,
    //! which is bold; empty is the default; call before `init()`
    void set_font_style(const std::string& style);

    //! Scale all metrics and fonts (relative to the default theme)
    void scale(double factor);

    //! Change the colors of all the styles (and the toolkit ones)
    void set_variant(Variant variant);

    uint32_t root_background = 0x729FCF;
    int win_header = 25;
    int win_icon = 21; // win_header - 2*header.padding
    int win_border = 10;
    int win_corner = win_border * 2;
    int win_min_width = 10;
    int win_min_height = 10 + win_header;

    int panel = 30;
    int icon = 48;
    int dock_border = 5;
    int dock_reveal = 5; //!< the strip at the screen edge that shows the dock
    int dock_attention_bar = 3; //!< under the icon that demands attention
    int panel_app_width = 200;
    int tray_icon = 18; //!< the side of an icon of the system tray
    int tray_gap = 4;   //!< the space at both sides of an icon of the tray
    int menu_width = 200;
    int menu_item_height = 20;
    int menu_separator = 8;     //!< the space of the line between item groups
    int popup_item_height = 30; //!< an item of a menu: as high as the panel
    int menu_padding = 12;      //!< left and right of the text of an item
    int dock_dot = 4;           //!< the diameter of a dot (a window)
    int dock_line = 1;          //!< the width of the line between the groups
    int dock_marker = 3;        //!< the width of the mark of a drop
    int dock_drag = 6;          //!< how far a press moves before it drags
    int launcher_columns = 8;
    int launcher_rows = 4;
    int launcher_cell_width = 64; // the narrowest cell
    int launcher_label = 40;      // under an icon, for its text
    int launcher_icon = 48;
    int launcher_dot = 8;
    int switcher_cell_width = 96;         //!< an application in Alt + Tab
    int switcher_window_cell_width = 160; //!< a window in Alt + Tab
    int switcher_window_icon = 128;       //!< the icon of a window there

    Theme& toolkit;

    ButtonStyle button;
    HeaderStyle header;
    DockStyle dock;
    PlainStyle dock_separator{0x555753, 0xC0BFBC};
    PlainStyle dock_mark{0x729FCF, 0x3584E4};

  private:
    //! The metrics and styles of the shell only
    void scale_own(double factor);
}; // ShellTheme

extern ShellTheme shell_theme;

} // namespace tiny::wm
