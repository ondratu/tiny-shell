#pragma once

#include <map>
#include <list>
#include <string>

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

    //! Requests of the clients to the root window (`_NET_ACTIVE_WINDOW`...)
    bool on_root_message(const XClientMessageEvent& e);

    bool is_super(unsigned int keycode) const;

    //! Restore, focus and remember as active at once
    void focus_window(Window* window);

    void activate_next_window();

    void activate_prev_window();

    //! The icon of the desktop entry of the window, for the dock
    void set_application_icon(Window* window);

    //! Start a command of the user menu; an empty one does nothing
    void run_system_command(const std::string& command);

    void connect_window(Window* window);

    //! Frame (or not) and register a client; null for a type that is not managed
    Window* manage(::Window child, const XWindowAttributes& attrs);

    //! Forget a client; `alive` is false when it is gone already
    void unmanage(::Window child, bool alive);

    //! Remember the struts of a dock (a window that is not framed)
    void track_dock(::Window dock);

    //! Forget a dock that is gone or hidden
    void untrack_dock(::Window dock);

    //! Compute the work area again; windows and _NET_WORKAREA follow it
    void update_work_area();

    /* signal event handlers */
    void on_window_drag(tiny::Object* o, const XEvent& e, void* data);
    void on_window_focus(tiny::Object* o, const XEvent& e, void* data);
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
    std::map<::Window, Struts> wm_docks; //!< struts of the docks of clients
    tiny::Rect work_area = {0, 0, 0, 0};

    std::map<::Window, Window*> wm_windows;
    std::list<Window*> wm_tops;
    std::list<Window*> wm_pager;
    bool key_done;
    bool super_down = false; //!< Super is held and nothing cancelled it
    bool running;

    // until there is a configuration the commands are the systemd ones
    std::string restart_command = "systemctl reboot";
    std::string shutdown_command = "systemctl poweroff";
};

} // namespace tiny::wm
