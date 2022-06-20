#pragma once

#include "containers.h"
#include "buttons.h"

namespace wm {

class AppMenu: public tiny::Popover {
  public:
    AppMenu();

    ~AppMenu();

    virtual void realize(::Window parent, int x = 0, int y = 0);
  private:
    //tiny::LabelButton urxvt;
};


class UserMenu: public tiny::Popover {
  public:
    UserMenu();

    ~UserMenu();

    virtual void realize(::Window parent, int x = 0, int y = 0);

    //utiny::LabelButton sleep;
    tiny::LabelButton restart;
    tiny::LabelButton shutdown;
    tiny::LabelButton logout;
};


class Panel: public tiny::Box {
  public:
    Panel();

    ~Panel();

    virtual void realize(::Window root, int x = 0, int y = 0);

  private:
    tiny::MenuButton btn_app;
    tiny::MenuButton btn_login;

  public:
    AppMenu menu_app;
    UserMenu menu_user;
};

}; // namespace wm
