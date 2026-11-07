#pragma once

#include <functional>
#include <string>
#include <vector>

#include "wm/dock.h"

namespace tiny::wm {

class Window;

//! What an item of the window menu does
enum class WindowMenuAction {
    Restore,
    Maximize,
    Minimize,
    Move,
    Resize,
    RollUp,
    RollDown,
    AlwaysOnTop,
    NotAlwaysOnTop,
    Close
};

struct WindowMenuItem {
    WindowMenuAction action;
    std::string label;
};

//! What the menu needs to know about a window to offer the right items
struct WindowMenuState {
    bool maximized = false;
    bool fullscreen = false;
    bool shaded = false;
    bool above = false;
    bool can_maximize = false;
    bool can_minimize = false;
    bool can_move = false;
    bool can_resize = false;
    bool can_shade = false;
    bool can_close = false;
};

//! The state of a window for its menu
WindowMenuState window_menu_state(const Window& window);

/**
 * @short The items of the window menu, in the order they are shown.
 *
 * Only what can be done is listed: the menu of a maximized window has no
 * Resize and one of a window that cannot be closed has no Close. The most
 * used item is first and Close is last.
 */
std::vector<WindowMenuItem> window_menu_items(const WindowMenuState& state);

/**
 * @short The menu of a window (title bar, `_GTK_SHOW_WINDOW_MENU`, Alt+Space).
 *
 * One menu is made for the shell and shown for one window at a time. It looks
 * like the menu of an icon of the dock.
 */
class WindowMenu {
  public:
    //! The menu lives on the root window
    void realize(::Window root);

    //! Open the menu of `window` at the pointer (below it, kept on screen)
    void popup(Window& window, int x_root, int y_root);

    //! Close the menu; does nothing when it is not open
    void close() { menu.unmap(); }

    bool is_open() const { return menu.get_maped(); }

    //! The window whose menu is open, none when it is closed
    const Window* target() const { return open_for; }

    //! The window is going away: its menu must not be open
    void forget(const Window* window);

    DockMenu& get_menu() { return menu; }

    //! An item was chosen (the menu is closed already)
    std::function<void(Window&, WindowMenuAction)> on_action;

  private:
    DockMenu menu;
    Window* open_for = nullptr;
    std::vector<WindowMenuItem> items;
};

} // namespace tiny::wm
