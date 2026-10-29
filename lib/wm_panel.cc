#include "wm_panel.h"

namespace wm {

AppMenu::AppMenu():
    tiny::Popover(10, 10)
{
    name = "wm_appmenu";
}

AppMenu::~AppMenu()
{}

void AppMenu::realize(::Window parent, int x, int y)
{
    tiny::Popover::realize(parent, x, y);
}


UserMenu::UserMenu():
    tiny::Popover(tiny::theme.wm_menu_width, tiny::theme.wm_menu_item_height*3),
    restart(tiny::theme.wm_menu_width, tiny::theme.wm_menu_item_height, "Restart"),
    shutdown(tiny::theme.wm_menu_width, tiny::theme.wm_menu_item_height, "Shutdown"),
    logout(tiny::theme.wm_menu_width, tiny::theme.wm_menu_item_height, "Logout")
{
    name = "wm_usermanu";
}

UserMenu::~UserMenu()
{}

void UserMenu::realize(::Window parent, int x, int y)
{
    tiny::Popover::realize(parent, x, y);
    push_start(&restart);
    push_start(&shutdown);
    push_start(&logout);
}


Panel::Panel():
    tiny::Box(tiny::Box::Type::Horizontal, 0, tiny::theme.wm_panel),
    btn_app(tiny::theme.wm_panel_app_width, tiny::theme.wm_panel, "Application"),
    btn_login(tiny::theme.wm_panel_login_width, tiny::theme.wm_panel, "Login"),
    menu_app(), menu_user()
{
    name = "wm_panel";
    //btn_app.set_text_colors(WM_PANEL_TEXT, WM_PANEL_TEXT);
    //btn_login.set_text_colors(WM_PANEL_TEXT, WM_PANEL_TEXT);
}

Panel::~Panel()
{}


void Panel::realize(::Window root, int x, int y)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    resize(attrs.width ,height);

    tiny::Box::realize(root, x, y);

    menu_app.realize(root);
    menu_user.realize(root);

    push_start(&btn_app);
    push_back(&btn_login);

    btn_app.set_popover(&menu_app);
    btn_login.set_popover(&menu_user);
}


} // namespace wm
