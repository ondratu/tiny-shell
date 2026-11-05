#include "wm/shell_theme.h"
#include "wm/panel.h"

#include "wm/dock.h"

namespace tiny::wm {

namespace {

//! The number of the items and of the lines between their groups
constexpr int main_menu_items = 8;
constexpr int main_menu_lines = 5;

} // namespace

MainMenu::MainMenu():
    tiny::Popover(shell_theme.menu_width,
                  shell_theme.popup_item_height * main_menu_items +
                          shell_theme.menu_separator * main_menu_lines),
    about(shell_theme.menu_width, shell_theme.popup_item_height,
          "About Tiny Shell"),
    settings(shell_theme.menu_width, shell_theme.popup_item_height, "Settings"),
    applications(shell_theme.menu_width, shell_theme.popup_item_height,
                 "Applications"),
    terminal(shell_theme.menu_width, shell_theme.popup_item_height, "Terminal"),
    force_quit(shell_theme.menu_width, shell_theme.popup_item_height,
               "Force Quit"),
    restart(shell_theme.menu_width, shell_theme.popup_item_height, "Restart"),
    shutdown(shell_theme.menu_width, shell_theme.popup_item_height, "Shutdown"),
    logout(shell_theme.menu_width, shell_theme.popup_item_height, "Logout")
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

    tiny::LabelButton* const groups[][2] = {
            {&about, nullptr},          {&settings, nullptr},
            {&applications, &terminal}, {&force_quit, nullptr},
            {&restart, &shutdown},      {&logout, nullptr}};
    const int separator = shell_theme.menu_separator;
    int top = 0;
    for (const auto& group : groups) {
        if (top > 0) { // the line is in the middle of the space between
            lines.push_back(std::make_unique<DockLine>(
                    shell_theme.dock_separator, shell_theme.menu_width,
                    shell_theme.dock_line));
            add(lines.back().get(), 0,
                top + (separator - shell_theme.dock_line) / 2);
            top += separator;
        }
        for (tiny::LabelButton* item : group) {
            if (item == nullptr) {
                continue;
            }
            item->set_text_padding(shell_theme.menu_padding);
            item->set_border(0);
            add(item, 0, top);
            top += shell_theme.popup_item_height;
        }
    }
}

LogoButton::LogoButton(int w, int h, const std::string& text_arg):
    tiny::MenuButton(w, h, text_arg)
{
    name = "logo_button";
}

LogoButton::~LogoButton() = default;

bool LogoButton::has_logo() const
{
    return icons != nullptr && !icons->find("start-here").empty();
}

void LogoButton::on_expose(const XEvent& e, void* data)
{
    if (!has_logo()) {
        return tiny::MenuButton::on_expose(e, data);
    }
    const uint8_t state = get_theme_state();
    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);
    icons->draw(window, "start-here", height * 3 / 5, get_style().get_fg(state),
                width, height);
}

Panel::Panel():
    tiny::Box(tiny::Box::Type::Horizontal, 0, shell_theme.panel),
    logo(shell_theme.panel_app_width, shell_theme.panel, "Main Menu")
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

    if (logo.has_logo()) { // a square with the logo instead of the text
        logo.resize(height, height);
    }
    push_start(&logo);

    logo.set_popover(&menu_main);
}

void Panel::add_applet(tiny::Widget* applet, Side side)
{
    if (!is_realized) {
        return error("tiny::wm::Panel must be realized");
    }
    if (applet == nullptr || applet->get_realized()) { // null, or added already
        return error("tiny::wm::Panel: wrong applet");
    }
    if (side == Side::Left) {
        push_start(applet);
    } else {
        push_back(applet);
    }
    if (is_maped) {
        applet->map_all();
    }
}

} // namespace tiny::wm
