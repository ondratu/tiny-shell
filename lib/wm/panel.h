#pragma once

#include <memory>
#include <vector>

#include "container.h"
#include "button.h"
#include "wm/symbolic_icons.h"

namespace tiny::wm {

class DockLine;

/**
 * @short The main menu of the panel: the programs of the session and the
 * ways to leave it.
 *
 * The items are in groups with a line between them: About; Settings;
 * Applications and Terminal; Force Quit; Restart and Shutdown; Logout. The
 * owner connects
 * `on_click` of the items (and hides the menu).
 */
class MainMenu : public tiny::Popover {
  public:
    MainMenu();

    ~MainMenu() override;

    void realize(::Window parent, int x = 0, int y = 0) override;

    tiny::LabelButton about;
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

/**
 * @short The button of the main menu: the logo of the shell that opens the
 * menu.
 *
 * It is drawn from the icon `start-here` in the color of the text. Without
 * the icon (the data files are not installed) it shows the text.
 */
class LogoButton : public tiny::MenuButton {
  public:
    LogoButton(int width, int height, const std::string& text);

    ~LogoButton() override;

    //! The icons to draw from, or null for the text; they must live longer
    void set_icons(SymbolicIcons* icons_arg) { icons = icons_arg; }

    //! Is there an icon to draw?
    bool has_logo() const;

  protected:
    void on_expose(const XEvent& e, void* data) override;

  private:
    SymbolicIcons* icons = nullptr;
};

class Panel : public tiny::Box {
  public:
    //! Where an applet goes: the left one after the main menu button
    enum class Side { Left, Right };

    Panel();

    ~Panel() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    //! The icons for the logo; call before `realize()`, they must live longer
    void set_icons(SymbolicIcons* icons) { logo.set_icons(icons); }

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
    LogoButton logo;

  public:
    MainMenu menu_main;
};

} // namespace tiny::wm
