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
    tiny::Popover(120, 20*3),
    restart(120, 20, "Restart"),
    shutdown(120, 20, "Shutdown"),
    logout(120, 20, "Logout")
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
    tiny::Box(tiny::Box::Type::Horizontal, 0, WM_PANEL,
              0, 0, WM_PANEL_BACKGROUND),
    btn_app(200, WM_PANEL, "Application"), btn_login(120, WM_PANEL, "Login"),
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
