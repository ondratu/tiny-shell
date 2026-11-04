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
    //! Where an applet goes: the left one after the main menu button
    enum class Side { Left, Right };

    Panel();

    ~Panel() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    /**
     * @short Put a small widget of a plugin (a clock, a monitor...) in the
     * panel.
     *
     * The panel must be realized. The applet is realized by the panel and
     * stays where it is put: the next one goes beside it, to the left from
     * the first at the right side. The caller owns it and must remove it
     * (`remove()`) or keep it alive as long as the panel.
     */
    void add_applet(tiny::Widget* applet, Side side = Side::Right);

  private:
    tiny::MenuButton btn_main;

  public:
    MainMenu menu_main;
};

} // namespace tiny::wm
