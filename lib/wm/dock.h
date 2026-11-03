#pragma once

#include <functional>
#include <string>
#include <vector>

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
    virtual void on_enter_notify(const XEvent& e, void* data);
    virtual void on_leave_notify(const XEvent& e, void* data);
    virtual void on_button_release(const XEvent& e, void* data);
    virtual void on_expose(const XEvent& e, void* data);
};

/**
 * @short The icon of a window, or of a favorite application, or both.
 *
 * A click focuses the window, a click on the icon of an application that does
 * not run only tells `on_click`. The right button tells `on_menu`.
 */
class DockIcon : public DockButton {
  public:
    /**
     * `task` is null for a favorite application that does not run. `app_id`
     * is the desktop file id of the application, the one of the window when it
     * is empty. `icon` is drawn when there is no window to take it from.
     */
    explicit DockIcon(Window* task, std::string app_id = {},
                      const Window::AppIcon& icon = {});
    ~DockIcon() override;

    Window* get_task() const { return task; }

    const std::string& get_app_id() const { return app_id; }

    tiny::Signal on_menu;

    /// Told +1 when a release of a button is being handled and -1 when it is
    /// done; the dock must not delete the icon in between. The last thing the
    /// icon does is the -1, so the call may delete it.
    std::function<void(int)> track;

    void set_events(long mask = 0) override;

  protected:
    void on_button_release(const XEvent& e, void* data) override;
    void on_expose(const XEvent& e, void* data) override;

    void handle_release(const XEvent& e, void* data);

    void draw_app_icon(const Window::AppIcon& app_icon);

    Window* task;
    std::string app_id;
    Window::AppIcon favorite_icon; //!< for the time a favorite has no window
    Picture target = None; //!< of the window, for the icon of the application
};

//! A favorite application of the dock, the icon is not owned
struct DockFavorite {
    std::string id; //!< desktop file id
    Window::AppIcon icon;
};

//! A place in the dock: an index into the favorites and one into the windows
struct DockSlot {
    int favorite = -1; //!< -1 for a window that is not at a favorite
    int task = -1;     //!< -1 for a favorite that does not run
};

/**
 * @short The places of the dock, from the left.
 *
 * The favorites come first in their order. A favorite takes the first window
 * of its application (`task_apps` are the desktop file ids of the windows, in
 * the order they came, empty when unknown); the other windows follow, in the
 * same order, also the ones of an application that has a window at its
 * favorite already.
 */
std::vector<DockSlot> arrange_dock(const std::vector<std::string>& favorites,
                                   const std::vector<std::string>& task_apps);

//! The menu of an icon: add the application to the favorites or remove it
class DockMenu : public tiny::Popover {
  public:
    DockMenu();
    ~DockMenu() override;

    void realize(::Window parent, int x = 0, int y = 0) override;

    tiny::LabelButton toggle;
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

    /// The item of the menu of an icon was chosen (the id of the application)
    std::function<void(const std::string&)> on_toggle_favorite;

    void realize(::Window parent, int x, int y) override;
    void resize(int width, int height) override;

    const tiny::Style& get_style() const override
    {
        return tiny::theme.wm_dock;
    }

    /// The button with the dots at the end of the dock (the launcher).
    DockButton& get_menu_button() { return icon; }

    /// The menu of the right button on an icon.
    DockMenu& get_menu() { return menu; }

  private:
    //! Make the icons again from the windows and the favorites
    void rebuild();

    void clear();

    //! An icon starts (+1) or ends (-1) handling a click
    void track_handler(int change);

    //! Show the menu of an icon above it
    void show_menu(const DockIcon& icon, const XButtonEvent& be);

    bool is_favorite(const std::string& id) const;

    void resize_to_children();

    DockButton icon;
    DockMenu menu;
    std::string menu_app; //!< the application of the open menu
    int handling = 0;     //!< the clicks being handled: no rebuild meanwhile
    bool rebuild_later = false;
    std::vector<DockFavorite> favorites;
    std::vector<Window*> tasks;   //!< the windows with an icon, as they came
    std::vector<DockIcon*> icons; //!< from the left, without the menu button
};

} // namespace tiny::wm
