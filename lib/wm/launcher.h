#pragma once

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "button.h"
#include "container.h"
#include "entry.h"
#include "icon_grid.h"
#include "wm/launcher_layout.h"
#include "xdg/application_index.h"
#include "xdg/icon_theme.h"

namespace tiny::wm {

/**
 * @short Super: a field for a search and the installed applications below it.
 *
 * The launcher covers the whole screen (see `LauncherLayout`), the panel and
 * the dock added by `keep_above()` stay over it. Every typed letter searches
 * the applications (see `xdg::search_applications`); the found ones are a grid
 * of icons in pages. The arrows move in the grid, Page Up and Page Down turn
 * the pages (so do the wheel and the buttons < and > at the sides of the grid,
 * shown when there is more than one page), Return or a click starts the
 * selected one, Escape closes the launcher. The applications are read when the
 * launcher opens for the first time, the icons when a cell is shown for the
 * first time. The layout follows the screen every time the launcher opens.
 */
class Launcher : public tiny::Popover {
  public:
    Launcher();

    ~Launcher() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    void map_all() override;

    void popup();

    /// Keep a window (the panel, the dock) above the launcher.
    void keep_above(tiny::Widget* widget) { above.push_back(widget); }

    /// The directory that is searched after $XDG_DATA_DIRS, even when the
    /// variable does not list it (e.g. "/usr/pkg/share:/usr/share" on NetBSD).
    void set_local_share(std::string dir) { local_share = std::move(dir); }

    void toggle();

    /// Close the launcher and forget the search.
    void close(Object* o, const XEvent& e, void* data);

    void search(Object* o, const XEvent& e, void* data);

    void move(Object* o, const XEvent& e, void* data);

    void run(Object* o, const XEvent& e, void* data);

    /// A click on one of the buttons of the pages.
    void turn(Object* o, const XEvent& e, void* data);

    //! A loaded icon; the launcher owns the X resources
    struct Icon {
        Pixmap pixmap = None;
        Picture picture = None;
        int width = 0;
        int height = 0;
    };

    /**
     * @short The icon of the application a window belongs to.
     *
     * The window is matched by its `WM_CLASS` (see
     * `xdg::find_application_of_window`), the size is `theme.wm_icon`. The
     * icon has no picture when no application or no icon is found. It lives
     * as long as the launcher.
     */
    const Icon& application_icon(std::string_view instance,
                                 std::string_view class_name);

  private:
    void relayout();
    void arrange(const LauncherLayout& next);
    void update_buttons();
    void free_icons();
    void load();
    void find();
    tiny::IconGrid::Item item_of(size_t row);
    const Icon& icon_of(const xdg::Application& app);
    Icon load_icon(const std::string& name, int size) const;
    static void free_icons(::Display* x_display,
                           std::map<std::string, Icon>& list);
    void remember(const std::string& id);

    tiny::Entry entry;
    tiny::IconGrid grid;
    tiny::LabelButton prev_page;
    tiny::LabelButton next_page;
    LauncherLayout layout;
    std::vector<tiny::Widget*> above;

    ::Window root_window = None;
    bool loaded = false;
    std::string local_share = xdg::local_share_dir;
    xdg::ApplicationIndex index;
    std::unique_ptr<xdg::IconTheme> icon_theme;
    std::map<std::string, Icon> icons;        //!< by the id of the application
    std::map<std::string, Icon> window_icons; //!< the same for the windows
    std::vector<size_t> found;                //!< indexes into the applications
    std::vector<std::string> recent;          //!< ids, the latest first
};

} // namespace tiny::wm
