#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "container.h"
#include "entry.h"
#include "icon_grid.h"
#include "xdg/application_index.h"
#include "xdg/icon_theme.h"

namespace tiny::wm {

/**
 * @short Super: a field for a search and the installed applications below it.
 *
 * Every typed letter searches the applications (see
 * `xdg::search_applications`); the found ones are a grid of icons in pages.
 * The arrows move in the grid, Page Up and Page Down turn the pages (so does
 * the wheel), Return or a click starts the selected one, Escape closes the
 * launcher. The applications are read when the launcher opens for the first
 * time, the icons when a cell is shown for the first time.
 */
class Launcher : public tiny::Popover {
  public:
    Launcher();

    ~Launcher() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    void popup();

    void toggle();

    /// Close the launcher and forget the search.
    void close(Object* o, const XEvent& e, void* data);

    void search(Object* o, const XEvent& e, void* data);

    void move(Object* o, const XEvent& e, void* data);

    void run(Object* o, const XEvent& e, void* data);

  private:
    struct Icon {
        Pixmap pixmap = None;
        Picture picture = None;
        int width = 0;
        int height = 0;
    };

    void place();
    void load();
    void find();
    tiny::IconGrid::Item item_of(size_t row);
    const Icon& icon_of(const xdg::Application& app);
    Icon load_icon(const std::string& name) const;
    void remember(const std::string& id);

    tiny::Entry entry;
    tiny::IconGrid grid;

    ::Window root_window = None;
    bool loaded = false;
    xdg::ApplicationIndex index;
    std::unique_ptr<xdg::IconTheme> icon_theme;
    std::map<std::string, Icon> icons; //!< by the id of the application
    std::vector<size_t> found;         //!< indexes into the applications
    std::vector<std::string> recent;   //!< ids, the latest first
};

} // namespace tiny::wm
