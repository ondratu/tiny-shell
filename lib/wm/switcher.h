#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "wm/switcher_model.h"
#include "wm/switcher_panel.h"
#include "wm/window.h"

namespace tiny::wm {

/**
 * @short The window switcher shown by Alt + Tab while Alt is held.
 *
 * A panel in the middle of the screen has an icon for every running
 * application (once, with a dot for each of its windows when there are more)
 * and its name. Under it a second panel with the windows of the application
 * with the cursor (bigger icons, the titles) is shown when the cursor is on
 * a window. See `SwitcherModel` for the rules of the cursor. Nothing is
 * switched here: the owner reads the chosen window with `close()` when Alt is
 * released, and gives the keyboard to the switcher meanwhile.
 */
class Switcher {
  public:
    Switcher();
    ~Switcher();

    Switcher(const Switcher&) = delete;
    Switcher& operator=(const Switcher&) = delete;

    //! Create the windows of the panels on the root window (not shown)
    void realize(::Window root);

    //! The monitor where the panels are, in its middle; without it the root
    //! window
    void set_screen(const tiny::Rect& area) { monitor = area; }

    //! The generic icon of an application without one; must live longer
    void set_icons(SymbolicIcons* icons);

    //! The name of the application of a window under its icon
    std::function<std::string(const Window&)> app_name;

    //! The icon of a window in a size (the text is left empty)
    std::function<SwitcherCell(const Window&, int)> icon_of;

    bool is_open() const { return model.has_value(); }

    /**
     * @short Show the switcher for these windows, the one used last first.
     *
     * Alt + Tab starts on the application used before (`windows == false`),
     * Alt + ; on the window used before in the current application. With
     * `backwards` it starts from the end.
     */
    void open(const std::vector<Window*>& windows_arg, bool by_windows,
              bool backwards);

    //! Tab and Shift + Tab: the next application
    void next_app(bool backwards);

    //! The arrows: left and right move, down shows the windows, up hides them
    enum class Arrow { Left, Right, Up, Down };
    void arrow(Arrow direction);

    //! The key for the windows (;): the next window of the application
    void next_window(bool backwards);

    //! Hide the switcher; the window with the cursor (null when it is closed)
    Window* close();

    //! The window is gone: the switcher is closed (nothing is switched)
    bool contains(const Window* window) const;

    const SwitcherModel* get_model() const { return model ? &*model : nullptr; }

    SwitcherPanel& get_apps_panel() { return apps; }
    SwitcherPanel& get_windows_panel() { return windows; }

  private:
    //! Show the cursor of the model in the panels
    void refresh(const SwitcherModel& state);

    //! Put the panels in the middle of the screen
    void place();

    void show_windows(const SwitcherModel& state);

    std::optional<SwitcherModel> model;
    std::vector<Window*> list; //!< the windows, as the model counts them
    SwitcherPanel apps;
    SwitcherPanel windows;
    std::optional<size_t> windows_of; //!< the application the list shows
    ::Window root = None;
    tiny::Rect monitor = {0, 0, 0, 0}; //!< set by the owner, empty: the root
    tiny::Rect screen = {0, 0, 0, 0};  //!< where the panels are now
};

} // namespace tiny::wm
