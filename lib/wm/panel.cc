#include "wm/panel.h"

#include "wm/dock.h"

namespace tiny::wm {

namespace {

//! The number of the items and of the lines between their groups
constexpr int main_menu_items = 7;
constexpr int main_menu_lines = 4;

} // namespace

MainMenu::MainMenu():
    tiny::Popover(tiny::theme.wm_menu_width,
                  tiny::theme.wm_popup_item_height * main_menu_items +
                          tiny::theme.wm_menu_separator * main_menu_lines),
    settings(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
             "Settings"),
    applications(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
                 "Applications"),
    terminal(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
             "Terminal"),
    force_quit(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
               "Force Quit"),
    restart(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
            "Restart"),
    shutdown(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
             "Shutdown"),
    logout(tiny::theme.wm_menu_width, tiny::theme.wm_popup_item_height,
           "Logout")
{
    name = "wm_mainmenu";
}

MainMenu::~MainMenu() = default;

void MainMenu::realize(::Window parent_arg, int x, int y)
{
    if (is_realized) { // the items and the lines are made once
        return;
    }
    tiny::Popover::realize(parent_arg, x, y);

    tiny::LabelButton* const groups[][2] = {{&settings, nullptr},
                                            {&applications, &terminal},
                                            {&force_quit, nullptr},
                                            {&restart, &shutdown},
                                            {&logout, nullptr}};
    const int separator = tiny::theme.wm_menu_separator;
    int top = 0;
    for (const auto& group : groups) {
        if (top > 0) { // the line is in the middle of the space between
            lines.push_back(std::make_unique<DockLine>(
                    tiny::theme.wm_dock_separator, tiny::theme.wm_menu_width,
                    tiny::theme.wm_dock_line));
            add(lines.back().get(), 0,
                top + (separator - tiny::theme.wm_dock_line) / 2);
            top += separator;
        }
        for (tiny::LabelButton* item : group) {
            if (item == nullptr) {
                continue;
            }
            item->set_text_padding(tiny::theme.wm_menu_padding);
            item->set_border(0);
            add(item, 0, top);
            top += tiny::theme.wm_popup_item_height;
        }
    }
}

Panel::Panel():
    tiny::Box(tiny::Box::Type::Horizontal, 0, tiny::theme.wm_panel),
    btn_main(tiny::theme.wm_panel_app_width, tiny::theme.wm_panel, "Main Menu")
{
    name = "wm_panel";
}

Panel::~Panel() = default;

void Panel::realize(::Window root, int x, int y)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    resize(attrs.width, height);

    tiny::Box::realize(root, x, y);

    menu_main.realize(root);

    push_start(&btn_main);

    btn_main.set_popover(&menu_main);
}

} // namespace tiny::wm
