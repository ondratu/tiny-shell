#pragma once

#include <memory>
#include <vector>

#include "container.h"
#include "button.h"

namespace tiny::wm {

class DockLine;

/**
 * @short The main menu of the panel: the programs of the session and the
 * ways to leave it.
 *
 * The items are in groups with a line between them: Settings; Applications
 * and Terminal; Force Quit; Restart and Shutdown; Logout. The owner connects
 * `on_click` of the items (and hides the menu).
 */
class MainMenu : public tiny::Popover {
  public:
    MainMenu();

    ~MainMenu() override;

    void realize(::Window parent, int x = 0, int y = 0) override;

    tiny::LabelButton settings;
    tiny::LabelButton applications;
    tiny::LabelButton terminal;
    tiny::LabelButton force_quit;
    tiny::LabelButton restart;
    tiny::LabelButton shutdown;
    tiny::LabelButton logout;

  private:
    std::vector<std::unique_ptr<DockLine>> lines;
};

class Panel : public tiny::Box {
  public:
    Panel();

    ~Panel() override;

    void realize(::Window root, int x = 0, int y = 0) override;

  private:
    tiny::MenuButton btn_main;

  public:
    MainMenu menu_main;
};

} // namespace tiny::wm
