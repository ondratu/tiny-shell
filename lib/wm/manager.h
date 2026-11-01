#pragma once

#include <map>
#include <list>

#include "object.h"
#include "wm/window.h"
#include "wm/panel.h"
#include "wm/launcher.h"
#include "wm/run_dialog.h"
#include "wm/dock.h"

namespace tiny::wm {

class Manager : public tiny::Object {
  public:
    Manager(::Display* display, ::Window root);

    ~Manager() override;

    void main_loop();

  private:
    void set_events();

    //! Grab (or release) the Super keys that open the launcher
    void grab_super(bool grab);

    void set_supported();

    bool is_super(unsigned int keycode) const;

    //! Restore, focus and remember as active at once
    void focus_window(Window* window);

    void activate_next_window();

    void activate_prev_window();

    //! The icon of the desktop entry of the window, for the dock
    void set_application_icon(Window* window);

    void connect_window(Window* window);

    //! Frame (or not) and register a client; null for a type that is not managed
    Window* manage(::Window child, const XWindowAttributes& attrs);

    //! Forget a client; `alive` is false when it is gone already
    void unmanage(::Window child, bool alive);

    /* signal event handlers */
    void on_window_drag(tiny::Object* o, const XEvent& e, void* data);
    void on_window_focus(tiny::Object* o, const XEvent& e, void* data);
    void on_logout(tiny::Object* o, const XEvent& e, void* data);
    void on_launcher(tiny::Object* o, const XEvent& e, void* data);

    /* direct event handlers */
    void on_key_press(const XKeyEvent& xkey);

    void on_key_release(const XKeyEvent& xkey);

    void on_unmap_notify(const XUnmapEvent& e);

    void on_destroy_notify(const XDestroyWindowEvent& e);

    void on_map_request(const XMapRequestEvent& e);

    void on_configure_request(const XConfigureRequestEvent& e);

    static int on_error(::Display* display, XErrorEvent* error);

    ::Display* display;
    ::Window root;
    XErrorHandler old_error_handler = nullptr;

    Panel wm_panel;
    RunDialog wm_run;
    Launcher wm_launcher;
    Dock wm_dock;

    Window* active = nullptr;

    std::map<::Window, Window*> wm_windows;
    std::list<Window*> wm_tops;
    std::list<Window*> wm_pager;
    bool key_done;
    bool super_down = false; //!< Super is held and nothing cancelled it
    bool running;
};

} // namespace tiny::wm
