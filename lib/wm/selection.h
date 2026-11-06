#pragma once

#include <chrono>
#include <string>

#include <X11/Xlib.h>

namespace tiny::wm {

//! The name of the selection that tells which window manager runs: `WM_S0`
std::string selection_name(int screen);

//! The name of the selection of the system tray (freedesktop.org):
//! `_NET_SYSTEM_TRAY_S0`
std::string tray_selection_name(int screen);

/**
 * @short The ownership of the `WM_Sn` selection (ICCCM 2.8).
 *
 * The window manager that owns it is "the" window manager of the screen.
 * Another one takes it over by asking for the selection (`--replace`): the
 * owner gets `SelectionClear` and is expected to exit, which is how the
 * manager is handed over without a restart of the session. The selection is
 * given back when the object is destroyed.
 */
class Selection {
  public:
    enum class Result {
        Acquired,
        Occupied, //!< someone owns it and we were not asked to replace it
        Timeout,  //!< the old owner did not go away in time
        Failed,   //!< someone else took it in the meantime
    };

    //! The `WM_Sn` selection of the screen
    Selection(::Display* display, ::Window root, int screen);

    //! Any manager selection (ICCCM 2.8) by its name
    Selection(::Display* display, ::Window root, const std::string& name);
    ~Selection();

    Selection(const Selection&) = delete;
    Selection& operator=(const Selection&) = delete;

    //! Take the selection and, if `replace`, wait until the old owner is gone
    Result acquire(bool replace, std::chrono::milliseconds timeout);

    //! Tell the clients we are the manager (after the root window is ours)
    void announce();

    //! The window that owns the selection: the clients send it messages
    ::Window get_window() const { return owner_window; }

    //! The selection of the `SelectionClear` event is the one we hold
    bool is_lost_by(const XSelectionClearEvent& e) const;

  private:
    //! The time of the server (the timestamp ICCCM wants) from an event
    Time server_time();
    //! Acquired when the old owner is gone, Timeout or Failed otherwise
    Result wait_for_destroy(::Window owner, std::chrono::milliseconds timeout);

    ::Display* display;
    ::Window root;
    Atom selection;
    ::Window owner_window; //!< the window that owns the selection
    Time acquired_at = CurrentTime;
};

} // namespace tiny::wm
