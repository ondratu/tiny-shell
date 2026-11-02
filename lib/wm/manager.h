#pragma once

#include <map>
#include <set>
#include <list>
#include <string>
#include <vector>

#include "object.h"
#include "wm/window.h"
#include "wm/panel.h"
#include "wm/launcher.h"
#include "wm/run_dialog.h"
#include "wm/dock.h"
#include "wm/work_area.h"

namespace tiny::wm {

class Manager : public tiny::Object {
  public:
    Manager(::Display* display, ::Window root);

    ~Manager() override;

    void main_loop();

  private:
    void set_events();

    //! SIGTERM, SIGHUP and SIGINT end the main loop like the logout does
    void catch_quit_signals();

    void release_quit_signals();

    //! Block until an event is queued; false when a signal asks to quit
    bool wait_for_event();

    //! Grab (or release) the Super keys that open the launcher
    void grab_super(bool grab);

    //! Publish `_NET_SUPPORTED` and the window that proves the manager runs
    void set_supported();

    //! Remove what `set_supported` and `publish_clients` put on the root
    void clear_published();

    //! Publish `_NET_CLIENT_LIST` and `_NET_ACTIVE_WINDOW`
    void publish_clients();

    //! `_NET_CLIENT_LIST_STACKING`, the lowest window first
    void publish_stacking();

    //! Windows that are above or below keep their layer (publish after)
    void restack_layers();

    //! `_NET_REQUEST_FRAME_EXTENTS`: tell a window that is not managed yet
    void request_frame_extents(::Window child);

    //! Requests of the clients to the root window (`_NET_ACTIVE_WINDOW`...)
    bool on_root_message(const XClientMessageEvent& e);

    bool is_super(unsigned int keycode) const;

    //! Restore, focus and remember as active at once
    void focus_window(Window* window);

    //! `window` has the focus now: tell the dock and the root properties
    void remember_active(Window* window, const XEvent& e);

    //! Windows Alt + Tab walks through: the ones that have no dialog open
    std::vector<Window*> switchable() const;

    void activate_next_window();

    void activate_prev_window();

    //! The window before `window` that is not minimized, null if none
    Window* previous_shown(const Window* window) const;

    //! The icon of the desktop entry of the window, for the dock
    void set_application_icon(Window* window);

    //! Start a command of the user menu; an empty one does nothing
    void run_system_command(const std::string& command);

    void connect_window(Window* window);

    //! The managed window that `window` is a dialog of, null if it is none
    Window* parent_of(const Window* window) const;

    //! A dialog of a managed window (not a toolbar, menu and the like)
    bool is_dialog(const Window* window) const;

    //! Windows that are dialogs of `window`
    std::vector<Window*> transients_of(const Window* window) const;

    //! Modal dialogs that keep the focus away from `window`: its modal
    //! transients and the modal windows without a parent of its group
    //! (EWMH `_NET_WM_STATE_MODAL`)
    std::vector<Window*> modal_dialogs_of(const Window* window) const;

    //! Put a dialog in the middle of its parent, inside the work area
    void center_over(Window* dialog, const Window* parent);

    //! Frame (or not) and register a client; null for a type that is not managed
    Window* manage(::Window child, const XWindowAttributes& attrs);

    //! Forget a client; `alive` is false when it is gone already
    //! `reframing`: the window comes back at once, so its dialogs stay
    void unmanage(::Window child, bool alive, bool reframing = false);

    //! Remember the struts of a dock (a window that is not framed)
    void track_dock(::Window dock);

    //! Forget a dock that is gone or hidden
    void untrack_dock(::Window dock);

    //! The top edge taken by our own panel
    Struts panel_strut() const;

    //! Write `_NET_WM_STRUT_PARTIAL` of the panel
    void publish_panel_strut();

    //! Compute the work area again; windows and _NET_WORKAREA follow it
    void update_work_area();

    //! Focus stealing prevention: may a window that was just mapped have it
    bool may_take_focus(Window* window) const;

    //! May an application activate the window with a request of that time
    bool may_activate(Window* window, unsigned long time) const;

    //! Remember `_NET_WM_USER_TIME` of the window as the time of the user
    void note_user_time(const Window* window);

    //! Add or remove the dock icon of the window, as its state wants
    void update_dock_entry(Window* window);

    /* signal event handlers */

    //! The decorations came or went: the window needs another kind of frame
    void on_window_reframe(const Window* window);

    //! Manage the windows that asked for another frame again
    void reframe_windows();

    void reframe(::Window child);
    void on_window_user_time(const Window* window);
    void on_window_modal(const Window* dialog);
    void on_window_focus(Window* window, const XEvent& e);
    void on_window_minimize(Window* window);
    void on_logout(tiny::Object* o, const XEvent& e, void* data);
    void on_restart(tiny::Object* o, const XEvent& e, void* data);
    void on_shutdown(tiny::Object* o, const XEvent& e, void* data);
    void on_launcher(tiny::Object* o, const XEvent& e, void* data);

    /* direct event handlers */
    void on_key_press(const XKeyEvent& xkey);

    void on_key_release(const XKeyEvent& xkey);

    void on_unmap_notify(const XUnmapEvent& e);

    void on_destroy_notify(const XDestroyWindowEvent& e);

    void on_property_notify(const XPropertyEvent& e);

    void on_map_request(const XMapRequestEvent& e);

    void on_configure_request(const XConfigureRequestEvent& e);

    static int on_error(::Display* display, XErrorEvent* error);

    ::Display* display;
    ::Window root;
    XErrorHandler old_error_handler = nullptr;
    ::Window check_window = None; //!< `_NET_SUPPORTING_WM_CHECK`

    Panel wm_panel;
    RunDialog wm_run;
    Launcher wm_launcher;
    Dock wm_dock;

    Window* active = nullptr;
    unsigned long last_user_time = 0; //!< of the last user action we know of
    bool has_user_time = false;
    std::map<::Window, Struts> wm_docks; //!< struts of the docks of clients
    std::set<::Window> wm_desktops;      //!< windows of the DESKTOP type
    tiny::Rect work_area = {0, 0, 0, 0};

    std::map<::Window, Window*> wm_windows;
    std::list<Window*> wm_tops;
    std::list<Window*> wm_pager;
    bool key_done;
    bool super_down = false; //!< Super is held and nothing cancelled it
    bool running;
    std::vector<::Window> reframes; //!< windows that need another frame
    //! The unmaps of this window in the serials [first, last) are our own
    struct {
        ::Window window;
        unsigned long first;
        unsigned long last;
    } reframed = {None, 0, 0};

    // until there is a configuration the commands are the systemd ones
    std::string restart_command = "systemctl reboot";
    std::string shutdown_command = "systemctl poweroff";
};

} // namespace tiny::wm
