#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "object.h"
#include "timers.h"
#include "wm/window.h"
#include "wm/clock_label.h"
#include "wm/panel.h"
#include "wm/action_registry.h"
#include "wm/config.h"
#include "wm/launcher.h"
#include "wm/run_dialog.h"
#include "wm/unresponsive_dialog.h"
#include "wm/dock.h"
#include "wm/dock_mode.h"
#include "wm/dock_reveal.h"
#include "wm/selection.h"
#include "wm/work_area.h"

namespace tiny::wm {

class Manager : public tiny::Object {
  public:
    Manager(::Display* display, ::Window root);

    ~Manager() override;

    void main_loop();

    //! The manager ends when another one takes this selection (`--replace`)
    void set_selection(const Selection* owned) { selection = owned; }

    //! Commands of Restart and Shutdown in the main menu; an empty one keeps
    //! the current command
    void set_power_commands(const std::string& restart,
                            const std::string& shutdown);

    //! The terminal of the applications that need one, e.g. `{"xterm", "-e"}`;
    //! the Terminal item of the main menu starts it without a trailing `-e`
    //! or `--`
    void set_terminal(std::vector<std::string> command)
    {
        terminal_program = command;
        // the option that takes a command (`-e`, `--`) is left out
        if (terminal_program.size() > 1 && (terminal_program.back() == "-e" ||
                                            terminal_program.back() == "--")) {
            terminal_program.pop_back();
        }
        wm_launcher.set_terminal(std::move(command));
    }

    //! The program of the Settings item of the main menu (`tiny-settings`
    //! is searched in PATH by default)
    void set_settings_command(std::string command)
    {
        settings_command = std::move(command);
    }

    /**
     * @short The program that draws the wallpaper, with its arguments.
     *
     * It is started now and each time the screen changes its size
     * (`tiny-wallpaper`; it reads the size itself). An empty command means no
     * wallpaper: the root window has the color of the theme, which changes
     * with the variant of the theme. The picture does not.
     */
    void set_wallpaper_command(std::vector<std::string> command);

    /**
     * @short Show the date and the time in the panel, at its right end.
     *
     * `format` is for `strftime()`; empty removes the clock. The clock is
     * updated by a timer at the start of every minute.
     */
    void set_clock_format(const std::string& format);

    /**
     * @short When the dock is shown, see `DockMode`.
     *
     * With `AutoHide` the dock is hidden while the active window covers it,
     * and shown again when the pointer reaches the bottom edge of the screen
     * (until it leaves the dock).
     */
    void set_dock_mode(DockMode mode);

    DockMode get_dock_mode() const { return dock_mode; }

    /**
     * @short The favorite applications of the dock, the ids of desktop files.
     *
     * Their icons stay in the dock when they do not run. An id that is not an
     * installed application is kept but not shown. `changed` is called with
     * the whole list when the user adds or removes a favorite (to save it).
     */
    void set_favorites(
            std::vector<std::string> ids,
            std::function<void(const std::vector<std::string>&)> changed = {});

    //! Timers of the main loop (a clock, delays); they run in the loop, so
    //! the functions must be short
    tiny::Timers& get_timers() { return timers; }

    /**
     * @short What the menus and the keys ask for, by name.
     *
     * The manager registers "settings", "applications", "launcher",
     * "run-dialog", "terminal", "force-quit", "restart", "shutdown", "logout",
     * "next-window", "prev-window", "theme-dark", "theme-light" and
     * "theme-toggle"; more can be added (a script, the key bindings).
     */
    ActionRegistry& get_actions() { return actions; }

    //! Change the colors of the shell and repaint what is shown
    void set_variant(Variant variant);

  private:
    void set_events();
    //! The actions of the menus and the keys
    void register_actions();

    //! SIGTERM, SIGHUP and SIGINT end the main loop like the logout does
    void catch_quit_signals();

    void release_quit_signals();

    //! Block until an event is queued; false when a signal or a timer asks to
    //! quit (the events that are queued then are not handled)
    bool wait_for_event();

    //! Milliseconds to the next ping, its deadline or the question to the
    //! user, -1 when nothing waits
    int ping_timeout_ms() const;

    //! Milliseconds the loop may sleep: the pings and the timers
    int next_timeout_ms() const;

    //! Ping the windows, mark those that did not answer in time and ask the
    //! user about the ones that do not respond for long
    void check_pings();

    //! Show the dialog for a window that does not respond
    void ask_to_quit(Window* window);

    //! Ask about the window that waits the longest, if it is still worth it
    void ask_next();

    //! Hide the dialog; the window answered, is gone or the user chose
    void close_unresponsive_dialog();

    //! Grab (or release) the Super keys that open the launcher
    void grab_super(bool grab);

    //! Publish `_NET_SUPPORTED` and the window that proves the manager runs
    void set_supported();

    //! The one desktop: `_NET_NUMBER_OF_DESKTOPS`, `_NET_CURRENT_DESKTOP`,
    //! `_NET_DESKTOP_GEOMETRY` and `_NET_DESKTOP_VIEWPORT`
    void publish_desktops();

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

    //! The application of the window (the id, the icon) for the dock
    void set_application_icon(Window* window);

    //! Give the dock the favorites that are installed
    void update_favorites();

    //! Add the application to the favorites, or remove it when it is one
    void toggle_favorite(const std::string& id);

    //! A favorite was dragged to a place among the shown favorites
    void move_favorite(const std::string& id, size_t place);

    //! Start a command of the main menu (Restart, Shutdown) by the shell; an
    //! empty one does nothing
    void run_system_command(const std::string& command);
    //! Start one program of the main menu without a shell and without
    //! splitting; an empty one does nothing, a failure is reported on stderr
    void run_program(const std::vector<std::string>& argv);

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
    //! `fresh`: the window was just mapped, so its position is a wish (a window
    //! that was there already is where it is)
    Window* manage(::Window child, const XWindowAttributes& attrs,
                   bool fresh = true);

    //! Forget a client; `alive` is false when it is gone already
    //! `reframing`: the window comes back at once, so its dialogs stay
    void unmanage(::Window child, bool alive, bool reframing = false);

    //! Give all the clients back to the root window, shown
    void release_windows();

    //! Remember the struts of a dock (a window that is not framed)
    void track_dock(::Window dock);

    //! Forget a dock that is gone or hidden
    void untrack_dock(::Window dock);

    //! The top edge taken by our own panel
    Struts panel_strut() const;

    //! Write `_NET_WM_STRUT_PARTIAL` of the panel
    void publish_panel_strut();

    //! Show the time and wait for the next minute
    void tick_clock();

    //! Compute the work area again; windows and _NET_WORKAREA follow it
    void update_work_area();

    //! Focus stealing prevention: may a window that was just mapped have it
    bool may_take_focus(const Window* window) const;

    //! May an application activate the window with a request of that time
    bool may_activate(const Window* window, unsigned long time) const;

    //! Remember `_NET_WM_USER_TIME` of the window as the time of the user
    void note_user_time(const Window* window);

    //! Show or hide the dock as its mode, the active window and the pointer
    //! want (see `DockMode`); cheap when nothing changed
    void update_dock_visibility();

    //! Add or remove the dock icon of the window, as its state wants
    void update_dock_entry(Window* window);

    /**
     * @short A client asks to be moved or resized (`_NET_WM_MOVERESIZE`).
     *
     * The manager grabs the pointer and drives the window until the button
     * is released. Nothing happens when no button is down (the client was
     * late) or the window may not be moved or resized that way.
     */
    void start_move_resize(Window* window, const XClientMessageEvent& e);

    //! Follow the pointer while a window is moved or resized
    void move_resize_to(int x_root, int y_root);

    //! Release the pointer; `finish` tells the window it is let go
    void stop_move_resize(bool finish = true);

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

    /* direct event handlers */
    void on_key_press(const XKeyEvent& xkey);

    void on_key_release(const XKeyEvent& xkey);

    void on_unmap_notify(const XUnmapEvent& e);

    void on_destroy_notify(const XDestroyWindowEvent& e);
    void on_selection_clear(const XSelectionClearEvent& e);

    void on_property_notify(const XPropertyEvent& e);
    void on_root_configure(const XConfigureEvent& e);

    void on_map_request(const XMapRequestEvent& e);

    void on_configure_request(const XConfigureRequestEvent& e);

    static int on_error(::Display* display, XErrorEvent* error);

    ::Display* display;
    ::Window root;
    XErrorHandler old_error_handler = nullptr;
    ::Window check_window = None; //!< `_NET_SUPPORTING_WM_CHECK`

    Panel wm_panel;
    std::unique_ptr<ClockLabel> wm_clock; //!< null: no clock
    tiny::Timers::Id clock_timer = 0;
    RunDialog wm_run;
    UnresponsiveDialog wm_unresponsive;
    Window* asked = nullptr; //!< the window of the unresponsive dialog
    Launcher wm_launcher;
    Dock wm_dock;
    DockReveal wm_dock_reveal{shell_theme.dock_reveal};
    DockMode dock_mode = DockMode::AutoHide;
    bool dock_revealed = false; //!< the pointer brought a hidden dock back

    Window* active = nullptr;
    Window* dragged = nullptr; //!< moved or resized by the manager
    unsigned drag_button = 0;  //!< its release ends the drag, 0: any
    //! Is a pointer button held (a test replaces it)
    std::function<bool()> is_button_down;
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
    const Selection* selection = nullptr;
    std::vector<::Window> reframes; //!< windows that need another frame
    //! The unmaps of this window in the serials [first, last) are our own
    struct {
        ::Window window;
        unsigned long first = 0;
        unsigned long last = 0;
    } reframed = {None, 0, 0};

    tiny::Timers timers;
    ActionRegistry actions;

    std::vector<std::string> favorites; //!< ids of desktop files
    std::function<void(const std::vector<std::string>&)> favorites_changed;

    // until there is a configuration the commands are the systemd ones
    std::string settings_command = "tiny-settings";
    std::vector<std::string> wallpaper_command; //!< empty: no wallpaper
    int screen_width = 0; //!< the root window, to see a change
    int screen_height = 0;
    std::string force_quit_program = "xkill";
    std::vector<std::string> terminal_program = {"xterm"};
    std::string restart_command = Config().reboot_command;
    std::string shutdown_command = Config().poweroff_command;
};

} // namespace tiny::wm
