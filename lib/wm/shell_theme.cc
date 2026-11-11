#include "wm/shell_theme.h"

namespace tiny::wm {

ShellTheme shell_theme;

// The toolkit theme may not be constructed yet (the order of the globals), so
// the constructor does not touch it: it has the scale 1 of its own
ShellTheme::ShellTheme(Theme& toolkit_arg): toolkit(toolkit_arg)
{
    scale_own(1);
}

void ShellTheme::scale(double factor)
{
    toolkit.scale(factor);
    scale_own(factor);
}

void ShellTheme::scale_own(double factor)
{

    win_header = scaled(25, factor);
    win_icon = scaled(21, factor);
    win_border = scaled(10, factor);
    win_corner = win_border * 2;
    win_min_width = scaled(10, factor);
    win_min_height = scaled(10, factor) + win_header;

    panel = scaled(30, factor);
    icon = scaled(48, factor);
    dock_border = scaled(5, factor);
    dock_reveal = scaled(5, factor);
    dock_attention_bar = scaled(3, factor);
    panel_app_width = scaled(200, factor);
    tray_icon = scaled(18, factor);
    tray_gap = scaled(4, factor);
    menu_width = scaled(200, factor);
    menu_item_height = scaled(20, factor);
    menu_separator = scaled(8, factor);
    popup_item_height = scaled(30, factor);
    menu_padding = scaled(12, factor);
    dock_dot = scaled(4, factor);
    dock_line = scaled(1, factor);
    dock_marker = scaled(3, factor);
    dock_drag = scaled(6, factor);
    launcher_cell_width = scaled(64, factor);
    launcher_label = scaled(40, factor);
    launcher_icon = scaled(48, factor);
    launcher_dot = scaled(8, factor);
    switcher_cell_width = scaled(96, factor);
    switcher_window_cell_width = scaled(160, factor);
    switcher_window_icon = scaled(128, factor);

    button.scale(factor);
    header.scale(factor);
    dock.scale(factor);
    panel_widgets.scale(factor);
    dock_widgets.scale(factor);
    launcher_widgets.scale(factor);
    dock_list.scale(factor);
}

void ShellTheme::set_font(const std::string& family)
{
    toolkit.set_font(family);
    button.set_font_family(family);
    header.set_font_family(family);
    panel_widgets.set_font_family(family);
    dock_widgets.set_font_family(family);
    launcher_widgets.set_font_family(family);
    dock_list.set_font_family(family);
}

void ShellTheme::set_font_size(double size)
{
    toolkit.set_font_size(size);
    button.set_font_size(size);
    header.set_font_size(size);
    panel_widgets.set_font_size(size);
    dock_widgets.set_font_size(size);
    launcher_widgets.set_font_size(size);
    dock_list.set_font_size(size);
}

void ShellTheme::set_font_style(const std::string& style)
{
    toolkit.set_font_style(style);
    button.set_font_style(xft_style(style));
    panel_widgets.set_font_style(xft_style(style));
    dock_widgets.set_font_style(xft_style(style));
    launcher_widgets.set_font_style(xft_style(style));
    dock_list.set_font_style(xft_style(style));
}

void ShellTheme::set_variant(Variant new_variant)
{
    variant = new_variant;
    toolkit.set_variant(variant);
    button.colorize(variant);
    header.colorize(variant);
    apply_looks();
}

void ShellTheme::set_looks(Look new_panel_look, Look new_dock_look,
                           Look new_launcher_look)
{
    panel_look = new_panel_look;
    dock_look = new_dock_look;
    launcher_look = new_launcher_look;
    apply_looks();
}

void ShellTheme::apply_looks()
{
    const Variant panel_colors = panel_variant();
    panel_widgets.colorize(panel_colors);
    panel_separator.colorize(panel_colors);
    const Variant dock_colors = dock_variant();
    dock.colorize(dock_colors);
    dock_widgets.colorize(dock_colors);
    dock_separator.colorize(dock_colors);
    dock_mark.colorize(dock_colors);
    dock_list.colorize(dock_colors);
    launcher_widgets.colorize(launcher_variant());
    // a widget that is not shown takes the colors when it is mapped
    ++toolkit.generation;
}

void ShellTheme::release()
{
    toolkit.release();
    button.release();
    header.release();
    panel_widgets.release();
    dock_widgets.release();
    launcher_widgets.release();
    dock_list.release();
}

void ShellTheme::init()
{
    toolkit.init();
    button.init();
    header.init();
    panel_widgets.init();
    dock_widgets.init();
    launcher_widgets.init();
    dock_list.init();
}

} // namespace tiny::wm
