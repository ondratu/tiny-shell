#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "wm/shell_theme.h"
#include "container.h"
#include "button.h"
#include "wm/window.h"

namespace tiny::wm {

class DockButton : public tiny::Widget {
  public:
    DockButton();
    ~DockButton() override;

    int get_border() const override { return 0; }

    void set_events(long mask = 0) override;

    void redraw(const XEvent& e) { on_expose(e, nullptr); }

    tiny::Signal on_click;

  protected:
    DockButton(int width, int height);

    virtual void on_enter_notify(const XEvent& e, void* data);
    virtual void on_leave_notify(const XEvent& e, void* data);
    virtual void on_button_release(const XEvent& e, void* data);
    virtual void on_expose(const XEvent& e, void* data);
};

/**
 * @short A line or a mark of one color: the line between the favorites and the
 * other applications, the mark of the place a dragged icon would take.
 */
class DockLine : public tiny::Widget {
  public:
    DockLine(const tiny::Style& style, int width, int height);

    int get_border() const override { return 0; }

    const tiny::Style& get_style() const override { return style; }

  private:
    const tiny::Style& style;
};

/**
 * @short The icon of an application: all its windows, or the application
 * alone when it is a favorite that does not run.
 *
 * Under the icon there is a dot for every window. A click focuses a window
 * (the next one of the application when one of them has the focus already), a
 * click on the icon of an application that does not run only tells
 * `on_click`. The right button tells `on_menu`. A favorite can be dragged
 * (see `set_draggable`): the press and a move of more than `wm_dock_drag`
 * start it, the icon is drawn smaller meanwhile, the release ends it and is
 * not a click.
 */
class DockIcon : public DockButton {
  public:
    /**
     * `app_id` is the desktop file id of the application, the one of the first
     * window when it is empty. `icon` is drawn when there is no window to take
     * it from.
     */
    explicit DockIcon(std::vector<Window*> windows, std::string app_id = {},
                      const Window::AppIcon& icon = {});

    //! The icon of one window (null: none), see above
    explicit DockIcon(Window* task, std::string app_id = {},
                      const Window::AppIcon& icon = {});
    ~DockIcon() override;

    //! The first window, null when the application does not run
    Window* get_task() const { return windows.empty() ? nullptr : windows[0]; }

    const std::vector<Window*>& get_windows() const { return windows; }

    const std::string& get_app_id() const { return app_id; }

    bool contains(const Window* window) const;

    //! Only a draggable icon starts a drag
    void set_draggable(bool value) { draggable = value; }

    bool is_draggable() const { return draggable; }

    bool is_dragging() const { return dragging; }

    tiny::Signal on_menu;

    /// The icon is dragged: told on every move (the root x of the pointer)
    std::function<void(const DockIcon&, int)> on_drag;

    /// The drag ended with the release of the button at this root x
    std::function<void(const DockIcon&, int)> on_drop;

    /// Escape cancelled the drag: nothing moves, the release that follows is
    /// not a click
    std::function<void(const DockIcon&)> on_cancel;

    /// Whether the drag was cancelled and its button is still down
    bool is_cancelled() const { return cancelled; }

    /// Told +1 when a release of a button is being handled and -1 when it is
    /// done; the dock must not delete the icon in between. The last thing the
    /// icon does is the -1, so the call may delete it.
    std::function<void(int)> track;

    void set_events(long mask = 0) override;

  protected:
    void on_button_press(const XEvent& e);
    void on_motion(const XEvent& e);
    void on_key_press(const XEvent& e);
    void on_button_release(const XEvent& e, void* data) override;
    void on_expose(const XEvent& e, void* data) override;

    void handle_release(const XEvent& e, void* data);

    //! The window the click focuses
    Window* next_window() const;

    void draw_app_icon(const Window::AppIcon& app_icon, bool small);

    //! A dot for every window under the icon
    void draw_dots(GC gc);

    std::vector<Window*> windows;
    std::string app_id;
    Window::AppIcon favorite_icon; //!< for the time a favorite has no window
    Picture target = None; //!< of the window, for the icon of the application
    bool draggable = false;
    bool pressed = false; //!< the left button is down on a draggable icon
    bool dragging = false;
    bool cancelled = false; //!< Escape ended the drag, the button is down yet
    int press_x = 0;        //!< the root x of the press
};

//! A favorite application of the dock, the icon is not owned
struct DockFavorite {
    std::string id; //!< desktop file id
    Window::AppIcon icon;
};

//! An icon of the dock: the windows of one application, a favorite or not
struct DockGroup {
    int favorite = -1; //!< index into the favorites, -1 for none
    std::string key;   //!< the favorite id or the key of the windows
    std::vector<size_t>
            tasks; //!< indexes of the windows, in the order they came
};

/**
 * @short The icons of the dock, from the left.
 *
 * The favorites come first in their order and take the windows with their key.
 * The other windows follow, the windows with one key in one icon, the icons in
 * the order of the first window. A window with an empty key (unknown
 * application) is an icon of its own. The keys of the windows are the desktop
 * file ids of their applications.
 */
std::vector<DockGroup> group_dock(const std::vector<std::string>& favorites,
                                  const std::vector<std::string>& task_keys);

//! Where a dragged favorite goes: the number of the other favorites whose
//! center (in x) is left of `x`
size_t drop_position(const std::vector<int>& centers, int x);

//! The text of a menu item for a title: at most `max_chars` characters, the
//! rest is replaced by an ellipsis; a title without text gets a name
std::string menu_title(std::string_view title, size_t max_chars);

//! The top left corner of a menu of `width` x `height` (with its border) that
//! opens above (or below) `y`, centered on `x` (or from it), kept in the screen
struct MenuPlace {
    int x;
    int y;
};
MenuPlace place_menu(int x, int y, int width, int height, bool above,
                     bool centered, int screen_width, int screen_height);

/**
 * @short The menu of an icon: a list of items, made again for every icon.
 *
 * Over the dock a click outside of the menu closes it (see `Popover`). It is
 * as wide as its longest text and a margin, an item is as high as the panel.
 */
class DockMenu : public tiny::Popover {
  public:
    DockMenu();
    ~DockMenu() override;

    /// Replace the items by new ones with these texts; the menu is as high as
    /// they are. Does nothing before the menu is realized.
    void set_items(const std::vector<std::string>& labels);

    size_t count() const { return items.size(); }

    tiny::LabelButton& item(size_t index) { return *items.at(index); }

    /// The size of the items together (the server tells the real one later)
    int content_width() const { return content_w; }
    int content_height() const { return content_h; }

    /// A click on an item (its index)
    std::function<void(size_t)> on_item;

    /// The menu was hidden
    std::function<void()> on_hide;

    void unmap() override;

  private:
    std::vector<std::unique_ptr<tiny::LabelButton>> items;
    int content_w = 0;
    int content_h = 0;
};

class Dock : public tiny::Box {
  public:
    Dock();
    ~Dock() override;

    using tiny::Box::add;
    using tiny::Box::remove;
    void add(Window* window);
    void remove(Window* window);

    //! The window has an icon in the dock
    bool contains(const Window* window) const;
    void redraw(const Window* window, const XEvent& e);

    /// The applications that stay in the dock when they do not run; the
    /// icons are made again.
    void set_favorites(std::vector<DockFavorite> list);

    const std::vector<DockFavorite>& get_favorites() const { return favorites; }

    /// A click on the icon of a favorite that does not run (its id)
    std::function<void(const std::string&)> on_launch;

    /// The item of the menu to add or remove a favorite was chosen
    std::function<void(const std::string&)> on_toggle_favorite;

    /// A favorite was dragged to another place: its id and the place among
    /// the other favorites (0 is the first). The owner changes the list and
    /// sets it again.
    std::function<void(const std::string&, size_t)> on_move_favorite;

    /// The menu (and its submenu) was closed
    std::function<void()> on_menu_closed;

    /// The names of the actions of an application for its menu; none when
    /// not set
    std::function<std::vector<std::string>(const std::string&)> list_actions;

    /// An action of the menu was chosen (the id of the application and the
    /// index in the list of its names)
    std::function<void(const std::string&, size_t)> on_action;

    /// Show the menu of an application at the pointer (below it), the same
    /// as the one of its icon; for the launcher.
    void popup_menu(const std::string& app_id, int x_root, int y_root);

    void realize(::Window parent, int x, int y) override;
    void resize(int width, int height) override;

    const tiny::Style& get_style() const override { return shell_theme.dock; }

    /// The button with the dots at the end of the dock (the launcher).
    DockButton& get_menu_button() { return icon; }

    /// The menu of the right button on an icon.
    DockMenu& get_menu() { return menu; }

    /// The submenu of the menu with the windows of the application.
    DockMenu& get_windows_menu() { return windows_menu; }

    /// The mark of the place a dragged icon would take (when it is mapped).
    DockLine& get_mark() { return mark; }

  private:
    //! Make the icons again from the windows and the favorites
    void rebuild();

    void clear();

    //! An icon starts (+1) or ends (-1) handling a click
    void track_handler(int change);

    //! Show the menu of an icon above it
    void show_menu(const DockIcon& icon, const XButtonEvent& be);

    //! Make and show the menu; `y` is where the menu starts or ends
    void open_menu(const std::string& app_id, std::vector<Window*> windows,
                   int x, int y, bool above);

    //! An item of the menu was chosen
    void choose(size_t index);

    //! The submenu with the windows, next to the item `index`
    void open_windows_menu(size_t index);

    void hide_menus();

    //! The windows of the application
    std::vector<Window*> windows_of(const std::string& app_id) const;

    //! The place of the favorite to drop a dragged icon at
    size_t drop_place(const DockIcon& dragged, int x_root) const;

    void drag(const DockIcon& dragged, int x_root);
    void drop(const DockIcon& dragged, int x_root);

    bool is_favorite(const std::string& id) const;

    void resize_to_children();

    DockButton icon;
    DockMenu menu;
    DockMenu windows_menu;
    DockLine mark;
    std::unique_ptr<DockLine> separator;

    enum class Item { NewWindow, Action, Windows, Favorite, Close };
    struct Entry {
        Item kind;
        size_t number; //!< the action
    };
    std::vector<Entry> entries;        //!< the items of the open menu
    std::vector<Window*> menu_windows; //!< the items of the submenu
    std::string menu_app;              //!< the application of the open menu
    MenuPlace menu_place = {0, 0};
    int handling = 0; //!< the clicks being handled: no rebuild meanwhile
    bool rebuild_later = false;
    std::vector<DockFavorite> favorites;
    std::vector<Window*> tasks;   //!< the windows with an icon, as they came
    std::vector<DockIcon*> icons; //!< from the left, without the menu button
    std::vector<int> icon_x;      //!< their places in the dock
};

} // namespace tiny::wm
