#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "wm/dock.h"

namespace tiny::wm {

/**
 * @short The cursor of the window switcher (Alt + Tab), without X.
 *
 * It is made from the application keys of the windows, the window used last
 * first (the key is the desktop file id; a window with an empty key is an
 * application of its own). The applications are in the order of their latest
 * window, each of them is there once.
 *
 * The cursor is on an application (`Level::Apps`, the list of its windows is
 * hidden) or on one of the windows of an application (`Level::Windows`, the
 * list is shown). Tab and Shift + Tab always go through the applications; in
 * the list of windows they show the windows of the application that gets the
 * cursor. The arrows left and right do the same on the applications and move
 * between the windows on the windows. Down goes from an application to its
 * first window, up goes back. Everything wraps around.
 *
 * The window is chosen at the end (`result()`): the first window of the
 * application, or the window with the cursor.
 */
class SwitcherModel {
  public:
    enum class Level { Apps, Windows };

    //! The keys of the windows, the most recently used window first
    explicit SwitcherModel(const std::vector<std::string>& keys);

    //! Alt + Tab: the cursor is on the application used before this one (on
    //! the last one when `backwards`)
    void start_apps(bool backwards);

    //! Alt + ;: the cursor is on the window of the current application used
    //! before this one (the last one when `backwards`)
    void start_windows(bool backwards);

    //! Tab and Shift + Tab: the next (or the previous) application
    void next_app(bool backwards);

    //! Left and right arrows: the same on applications, the next (or the
    //! previous) window of the application on windows
    void move(bool backwards);

    //! The key for windows (;): the next (or the previous) window of the
    //! application, the list of the windows is shown
    void next_window(bool backwards);

    //! Down: the list of the windows of the application, on the first one
    void down();

    //! Up: back to the application
    void up();

    Level get_level() const { return level; }

    //! The application with the cursor (index into `apps()`)
    size_t get_app() const { return app; }

    //! The window with the cursor in its application (index into the tasks of
    //! the group); 0 on the level of applications
    size_t get_window() const { return window; }

    //! The applications, the one used last first; `tasks` are indexes into
    //! the keys the model was made from
    const std::vector<DockGroup>& apps() const { return groups; }

    //! The windows of the application with the cursor
    const std::vector<size_t>& app_windows() const;

    //! The window with the cursor (an index into the keys), none when there is
    //! no window; 0 is the window used last, which the owner knows to be the
    //! current one or not
    std::optional<size_t> result() const;

  private:
    std::vector<DockGroup> groups;
    Level level = Level::Apps;
    size_t app = 0;
    size_t window = 0;
};

} // namespace tiny::wm
