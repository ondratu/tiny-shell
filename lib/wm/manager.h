#pragma once

#include <sys/types.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "display_power.h"
#include "object.h"
#include "timers.h"
#include "wm/window.h"
#include "wm/recent_list.h"
#include "wm/clock_label.h"
#include "wm/idle_lock.h"
#include "wm/idle_time.h"
#include "wm/screen_blank.h"
#include "wm/lock_guard.h"
#include "wm/panel.h"
#include "wm/action_registry.h"
#include "wm/config.h"
#include "wm/launcher.h"
#include "wm/monitor_label.h"
#include "wm/monitors.h"
#include "wm/startup.h"
#include "wm/run_dialog.h"
#include "wm/switcher.h"
#include "wm/session_dialog.h"
#include "wm/session_end.h"
#include "wm/unresponsive_dialog.h"
#include "wm/symbolic_icons.h"
#include "wm/dock.h"
#include "wm/dock_mode.h"
#include "wm/dock_reveal.h"
#include "wm/window_menu.h"
#include "wm/selection.h"
#include "wm/tray.h"
#include "wm/xsettings.h"
#include "wm/work_area.h"

namespace tiny::wm {

class Manager : public tiny::Object {
  public:
    Manager(::Display* display, ::Window root);

    ~Manager() override;

    void main_loop();

    //! What SIGHUP (and `tiny-shell --reload`) does: the settings are read
    //! again. Without it SIGHUP ends the shell like SIGTERM.
    void set_reload(std::function<void()> handler)
    {
        reload = std::move(handler);
    }

    //! The manager ends when another one takes this selection (`--replace`)
    void set_selection(const Selection* owned) { selection = owned; }

    //! Commands of Restart and Shutdown in the main menu; an empty one keeps
    //! the current command
    void set_power_commands(const std::string& restart,
                            const std::string& shutdown);

    //! Ask before the logout, the restart and the shutdown (`[session]
    //! confirm`); the applications are asked to close either way
    void set_session_confirm(bool ask) { session_confirm = ask; }

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

    //! The program of the About item (`tiny-about` is searched in PATH by
    //! default)
    void set_about_command(std::string command)
    {
        about_command = std::move(command);
    }

    //! The command that locks the screen (`tiny-lock`, searched in PATH by
    //! default). Super + L, the Lock Screen item and the action "lock" run it,
    //! and the manager starts it again if it dies without unlocking.
    void set_lock_command(std::string command)
    {
        lock_command = std::move(command);
    }

    /**
     * Lock the screen this long after the display is switched off
     * (`[lock] delay`); zero is at once, less than zero is never. The display
     * goes off after `[display] off_after` (see `set_display_off()`, or the
     * time the X server has when that is not set); when it never goes off the
     * delay counts from the last input, and zero then locks nothing. Without
     * the counter of the idle time in the X server nothing is locked and a
     * message says so, once.
     */
    void set_lock_delay(std::chrono::minutes delay);

    /**
     * Switch the display off after this long without input
     * (`[display] off_after`); zero keeps it on, less than zero leaves the
     * X server as it is (and puts back what it had if the shell has changed
     * it). The X server counts the time for DPMS; the shell covers the screen
     * with a black window at the same time, for the servers where DPMS does
     * nothing (see `set_dpms()`). Without DPMS in the server a message says so,
     * once.
     */
    void set_display_off(std::chrono::minutes after);

    /**
     * Whether the shell sets the time of DPMS in the X server (`[display]
     * dpms`, yes by default). With no, DPMS is left as it is (and put back if
     * the shell has changed it), and only the black window of the shell turns
     * the display off: for the drivers where DPMS does nothing or hangs the
     * machine. Call it before `set_display_off()`.
     */
    void set_dpms(bool use);

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
     *
     * With `follows_theme` (the wallpaper is the default one of the style)
     * the program is started again when the variant changes, with
     * `--style dark` or `--style light`.
     */
    void set_wallpaper_command(std::vector<std::string> command,
                               bool follows_theme = false);

    /**
     * @short Show the date and the time in the panel, at its right end.
     *
     * `format` is for `strftime()`; empty removes the clock. The clock is
     * updated by a timer at the start of every minute.
     */
    void set_clock_format(const std::string& format);

    /**
     * @short Become the system tray: the icons of the programs go to the
     * panel, to the left from the clock.
     *
     * Call it after `set_clock_format()`, so that the clock stays at the
     * end. When another tray runs, there is no tray here (and a message).
     * The tray is gone when another one takes the selection.
     */
    void set_system_tray(bool enable);

    /**
     * Tell the programs the look through the selection `_XSETTINGS_Sn`
     * (XSETTINGS): `make` gives the settings for a variant of the theme and is
     * asked again when the variant changes. Nothing is published when `enable`
     * is false, or when another manager of settings runs (it is not replaced).
     */
    void
    set_xsettings(bool enable,
                  std::function<std::vector<XSetting>(Variant)> make = nullptr);

    //! The manager of the settings, null when there is none
    const XSettings* get_xsettings() const { return wm_xsettings.get(); }

    //! The icons of the tray, null when there is no tray
    const Tray* get_tray() const { return wm_tray.get(); }

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
     * The manager registers "about", "settings", "applications", "launcher",
     * "run-dialog", "terminal", "force-quit", "restart", "shutdown", "logout",
     * "next-window", "prev-window", "theme-dark", "theme-light",
     * "theme-toggle"; more can be added (a script, the key bindings).
     */
    ActionRegistry& get_actions() { return actions; }

    //! Change the colors of the shell and repaint what is shown
    void set_variant(Variant variant);

    //! The look of the panel and of the dock (`Auto` follows the variant)
    void set_looks(Look panel_look, Look dock_look, Look launcher_look);

    /**
     * @short Which monitors the shell uses, see `[monitors]` of the
     * configuration.
     *
     * `primary` is the monitor of the panel, the dock, the launcher and the
     * dialogs: a number as the monitor is numbered (from the left, then from
     * the top), the name of an output, or "auto" (the primary monitor of
     * RandR). `secondary` is the other one that the placement refers to
     * ("auto": the first one that is not the primary). The placement runs
     * `xrandr` to put the secondary monitor on one side of the primary one;
     * `Keep` leaves the layout as it is.
     */
    void set_monitors(const std::string& primary, const std::string& secondary,
                      Placement where);

    //! Show the number of every monitor in its corner (until they are hidden)
    void show_monitor_numbers();

    //! Hide the numbers of the monitors
    void hide_monitor_numbers();

    const MonitorLabels& get_monitor_labels() const { return monitor_labels; }

    //! The monitors, numbered from the left to the right
    const Monitors& get_monitors() const { return monitors; }

    //! The monitor of the panel and the dock (an index of `get_monitors()`)
    size_t get_primary_monitor() const { return choice.primary; }

    //! The work area of each monitor, in the order of `get_monitors()`
    const std::vector<tiny::Rect>& get_work_areas() const { return work_areas; }

  private:
    void set_events();
    //! The actions of the menus and the keys
    void register_actions();

    //! SIGTERM and SIGINT end the main loop like the logout does, SIGHUP
    //! calls `reload`
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

    /**
     * @short The shell starts an application that tells when it has started
     * (`StartupNotify`): the id for `DESKTOP_STARTUP_ID`.
     *
     * The "busy" cursor shows until its first window comes, the application
     * (or the launcher) says it is done, or `StartupNotification::timeout`.
     */
    std::string begin_startup(const xdg::Application& app);

    //! A window came: the application it belongs to has started
    void note_started(::Window child);

    //! Drop the sequences that took too long
    void check_startup();

    //! The cursor of the root window is the "busy" one while something starts
    void update_busy_cursor();

    //! Ping the windows, mark those that did not answer in time and ask the
    //! user about the ones that do not respond for long
    void check_pings();

    //! Show the dialog for a window that does not respond
    void ask_to_quit(Window* window);

    //! Ask about the window that waits the longest, if it is still worth it
    void ask_next();

    //! Hide the dialog; the window answered, is gone or the user chose
    void close_unresponsive_dialog();

    //! Publish `_NET_SUPPORTED` and the window that proves the manager runs
    void set_supported();

    //! The one desktop: `_NET_NUMBER_OF_DESKTOPS`, `_NET_CURRENT_DESKTOP`,
    //! `_NET_DESKTOP_NAMES`,
    //! `_NET_DESKTOP_GEOMETRY` and `_NET_DESKTOP_VIEWPORT`
    void publish_desktops();

    /**
     * @short `_NET_SHOWING_DESKTOP`: hide all the windows to see the desktop,
     * or bring back the ones that were hidden.
     *
     * Windows that were minimized before stay minimized. Focusing a window
     * (a new one, the dock, Alt + Tab) ends the mode and brings the others
     * back, too.
     */
    void show_desktop(bool show);

    //! `_NET_WM_FULLSCREEN_MONITORS`: the fullscreen window covers the
    //! monitors that the message names
    void set_fullscreen_monitors(Window* window, const XClientMessageEvent& e);

    //! `_NET_RESTACK_WINDOW`: put the window over or under a sibling, or
    //! on top or at the bottom when there is none
    void restack_window(Window* window, const XClientMessageEvent& e);

    //! Write `_NET_SHOWING_DESKTOP` on the root window
    void publish_showing_desktop();

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

    bool is_alt(unsigned int keycode) const;

    //! Grab (or release) the keys of the shortcuts of the manager: Alt + Tab,
    //! Alt + F2, Super, and Alt + ; on every key that makes a semicolon with
    //! no modifier in any group of the keyboard (the layout can have it on
    //! another key; a key that makes it only with Shift or AltGr is left
    //! alone). The keys of the last grab are released first, so it also
    //! follows a change of the layout.
    void grab_shortcuts(bool grab);

    //! The keyboard layout changed: the keys of the shortcuts are other ones
    void on_mapping_notify(XMappingEvent& e);

    bool is_semicolon(unsigned int keycode) const;

    //! Alt + Tab and Alt + ;: show the switcher and take the keyboard until
    //! Alt is released (when it is not held any more, switch at once)
    void open_switcher(bool windows, bool backwards);

    //! A key pressed while the switcher is shown
    void on_switcher_key(const XKeyEvent& xkey);

    //! Hide the switcher and give the keyboard back; `switch_now` focuses the
    //! window that was chosen, otherwise nothing changes
    void close_switcher(bool switch_now);

    //! The name under the icon of an application in the switcher
    std::string switcher_name(const Window& window);

    //! The icon of a window in a size for the switcher
    SwitcherCell switcher_icon(const Window& window, int size);

    //! Restore, focus and remember as active at once
    void focus_window(Window* window);

    //! `window` has the focus now: tell the dock and the root properties
    void remember_active(Window* window, const XEvent& e);

    //! Has the window an icon in the dock (and a place in Alt + Tab)
    bool is_listed(const Window* window) const;

    //! The listed windows, the one used last first
    std::vector<Window*> recent_windows() const;

    void activate_next_window();

    void activate_prev_window();

    //! The window used before `window` that is not minimized, null if none
    Window* previous_shown(const Window* window) const;

    //! The application of the window (the id, the icon) for the dock
    void set_application_icon(Window* window);

    //! Start the program of the wallpaper (nothing when there is none)
    void run_wallpaper();

    //! Repaint everything that is shown with the colors of the theme
    void restyle_widgets();

    //! Give the dock the favorites that are installed
    void update_favorites();

    //! Add the application to the favorites, or remove it when it is one
    void toggle_favorite(const std::string& id);

    //! A favorite was dragged to a place among the shown favorites
    void move_favorite(const std::string& id, size_t place);

    //! Start a command of the main menu (Restart, Shutdown) by the shell; an
    //! empty one does nothing. Returns the pid, -1 when nothing was started.
    pid_t run_system_command(const std::string& command);

    /**
     * Logout, restart and shutdown. The user is asked (a countdown of a
     * minute, Return and Escape work), then every window is asked to close
     * (`WM_DELETE_WINDOW`) so that the applications can ask for the unsaved
     * work, and the shell waits for them. Only then the shell ends, or the
     * command runs. "... Anyway" does not wait for what is left, Cancel stops
     * it at any time.
     */
    void end_session(SessionKind kind);
    //! The user agreed (or did not answer): ask the windows to close
    void session_proceed();
    //! The user changed their mind
    void session_cancel();
    //! The windows are closed, or the user does not wait: end the session
    void session_finish();
    //! Finish the session when no application is open, else say which are
    void session_check_windows();
    //! The timer of the countdown and of the wait for the windows
    void session_tick();
    //! "... Anyway" ends the session at once, the other button asks the windows
    void session_proceed_or_finish();
    //! Whether the end of the session waits for the window: an application
    //! that can be asked to close, not a bar, the desktop or a tooltip
    static bool session_counts(const Window& window);
    //! The names of the applications that are still open, once each
    std::vector<std::string> session_open_applications();
    //! Return agrees and Escape cancels, while the question is shown
    void session_key(const XKeyEvent& e);
    //! Watch the command of a restart or a shutdown: a failure is shown
    void session_watch_command();
    void session_release_keyboard();
    //! Start one program of the main menu without a shell and without
    //! splitting; an empty one does nothing, a failure is reported on stderr
    void run_program(const std::vector<std::string>& argv);

    //! Lock the screen and watch the lock (see `LockGuard`)
    void lock_screen();
    //! Look at the idle time and set the timer for the next look
    void check_idle_lock();
    void update_idle_lock();
    void check_screen_blank();
    void update_screen_blank();
    bool idle_counter(); //!< makes `idle_time`; false without the counter
    void apply_display_power(std::chrono::minutes after);
    tiny::DisplayPower& power();

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

    //! Read the monitors and use them: the shell goes to the primary one, the
    //! windows to the monitors they are on. `force` does it also when the
    //! monitors are the same.
    void update_monitors(bool force = false);

    //! Put the panel, the dock and the dialogs on the primary monitor
    void arrange_shell();

    //! The areas of the monitor that has most of the frame
    Window::MonitorAreas areas_at(const tiny::Rect& frame) const;

    //! The monitor with the pointer
    size_t pointer_monitor() const;

    //! The monitors changed (an event of RandR)
    void on_monitors_changed(const XEvent& e);

    //! Focus stealing prevention: may a window that was just mapped have it
    bool may_take_focus(const Window* window) const;

    //! May an application activate the window with a request of that time
    bool may_activate(const Window* window, unsigned long time) const;

    //! Remember `_NET_WM_USER_TIME` of the window as the time of the user
    void note_user_time(const Window* window);

    //! Where the dock is (it is centered at the bottom of the screen)
    tiny::Rect dock_area() const;

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

    /**
     * @short Move or resize a window with the pointer until a click (the
     * window menu).
     *
     * The window follows the pointer; a click of any of the three buttons
     * ends it and Escape puts the window back. A move needs no motion of the
     * pointer first, the window keeps its distance from it. A resize puts
     * the pointer on the bottom right corner of the window, which then
     * follows it. Nothing happens when the window
     * may not be moved or resized, or the pointer is held by something else.
     */
    void start_click_drag(Window* window, bool resize);

    //! May the pointer move (no edges) or resize the window by `edges`
    static bool may_drag(const Window& window, unsigned edges);

    /**
     * The first motion of a resize from the menu: the pointer is at the
     * corner when the warp worked. When it is not, the drag starts from
     * where the pointer is. False when the event only set the start.
     */
    bool check_warp(const XMotionEvent& motion);

    //! A click, a key or the end: finish (`cancel` false) or undo the drag
    void end_click_drag(bool cancel);

    //! Open the window menu of `window` at the pointer
    void open_window_menu(Window* window, int x_root, int y_root);

    //! The window menu of the active window, below its title bar
    void open_active_window_menu();

    //! An item of the window menu was chosen
    void run_window_menu_action(Window& window, WindowMenuAction action);

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
    StartupNotification startup;  //!< the applications that are starting
    unsigned startup_serial = 0;  //!< makes the ids of the shell different
    Cursor arrow_cursor = None;   //!< of the root window
    Cursor busy_cursor = None;    //!< while an application starts
    bool cursor_busy = false;     //!< the root window has `busy_cursor`

    SymbolicIcons wm_icons{display, root, xdg::default_data_dirs()};
    Panel wm_panel;
    std::unique_ptr<ClockLabel> wm_clock; //!< null: no clock
    tiny::Timers::Id clock_timer = 0;
    std::unique_ptr<Tray> wm_tray; //!< null: no tray
    RunDialog wm_run;
    UnresponsiveDialog wm_unresponsive;
    SessionDialog wm_session;
    SessionEnd session;
    bool session_confirm = true;
    tiny::Timers::Id session_timer = 0;
    bool session_keyboard = false; //!< the question has the keyboard
    //! The command of the restart or the shutdown that runs, to see if it fails
    pid_t session_command_pid = -1;
    std::string session_command_line;
    SessionKind session_command_kind = SessionKind::Restart;
    //! How long a failure of the command is watched for: it ends the session
    //! of the user, or it fails at once (no right, no such program)
    static constexpr std::chrono::seconds session_command_wait{10};
    SessionEnd::Clock::time_point session_command_at;
    Window* asked = nullptr; //!< the window of the unresponsive dialog
    Launcher wm_launcher;
    Switcher wm_switcher;
    Dock wm_dock;
    WindowMenu wm_window_menu; //!< of the title bar, Alt + Space, a client
    DockReveal wm_dock_reveal{shell_theme.dock_reveal};
    DockMode dock_mode = DockMode::AutoHide;
    bool dock_revealed = false; //!< the pointer brought a hidden dock back

    Window* active = nullptr;
    Window* dragged = nullptr; //!< moved or resized by the manager
    unsigned drag_button = 0;  //!< its release ends the drag, 0: any
    bool drag_by_click =
            false; //!< a click ends the drag, see `start_click_drag`
    bool release_pending = false; //!< the release of the click that ended it
    bool check_anchor = false;    //!< the first motion tells if the warp worked
    unsigned click_edges = 0;     //!< of the click drag, 0: a move
    int drag_start_x = 0;         //!< where the pointer was (Escape goes back)
    int drag_start_y = 0;
    //! Is a pointer button held (a test replaces it)
    std::function<bool()> is_button_down;
    struct GrabbedKey {
        KeyCode code = 0;
        unsigned int modifiers = 0;
    };
    std::vector<GrabbedKey> shortcut_keys; //!< what `grab_shortcuts` holds
    std::vector<KeyCode> semicolons;       //!< the keys grabbed for Alt + ;
    bool switcher_grabbed = false; //!< the keyboard is ours for the switcher
    //! Is Alt held (a test replaces it)
    std::function<bool()> is_alt_down;
    bool showing_desktop = false;         //!< see `show_desktop()`
    std::vector<::Window> desktop_hidden; //!< clients that it minimized
    unsigned long last_user_time = 0; //!< of the last user action we know of
    bool has_user_time = false;
    std::map<::Window, Struts> wm_docks; //!< struts of the docks of clients
    std::set<::Window> wm_desktops;      //!< windows of the DESKTOP type
    //! Windows of the popup types (menus, tooltips, notifications): above
    //! the others, never framed or focused
    std::set<::Window> wm_popups;
    tiny::Rect work_area = {0, 0, 0, 0}; //!< of the primary monitor
    std::vector<tiny::Rect> work_areas;  //!< of every monitor
    Monitors monitors;
    MonitorChoice choice;
    std::string primary_spec = "auto";
    std::string secondary_spec = "auto";
    Placement placement = Placement::Keep;
    std::vector<std::string> placement_asked; //!< the last xrandr we started
    //! The outputs that the placement moves, found once: a number names
    //! another monitor after the layout changes
    std::string placed_primary;
    std::string placed_secondary;
    std::optional<int> randr_base;         //!< RandR events, none: no RandR
    MonitorLabels monitor_labels;          //!< the numbers on the monitors
    ::Window monitor_numbers_owner = None; //!< shows them while it lives

    std::map<::Window, Window*> wm_windows;
    std::list<Window*> wm_tops;    //!< in the order they came
    RecentList<Window*> wm_recent; //!< in the order they were focused
    std::list<Window*> wm_pager;
    bool key_done = false;
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
    std::string about_command = "tiny-about";
    std::string lock_command = Config().lock_command;
    std::unique_ptr<LockGuard> lock_guard;
    std::unique_ptr<IdleTime> idle_time; //!< made with the first limit
    std::unique_ptr<IdleLock> idle_lock;
    tiny::Timers::Id idle_timer = 0;
    std::unique_ptr<ScreenBlank> screen_blank; //!< made when needed
    std::unique_ptr<IdleLock> blank_lock;      //!< when to cover the screen
    tiny::Timers::Id blank_timer = 0;
    std::chrono::minutes lock_delay{-1};  //!< [lock] delay, less than 0 never
    std::chrono::minutes display_off{-1}; //!< [display] off_after
    bool use_dpms = true;                 //!< [display] dpms
    std::unique_ptr<tiny::DisplayPower> display_power; //!< made when needed
    //! What the X server had before the shell changed it (the time, empty
    //! for DPMS off); "Unchanged" puts it back
    std::optional<std::optional<std::chrono::seconds>> display_power_before;
    tiny::Timers::Id lock_timer = 0;            //!< while the lock is watched
    std::vector<std::string> wallpaper_command; //!< empty: no wallpaper
    bool wallpaper_follows_theme = false;
    int screen_width = 0; //!< the root window, to see a change
    int screen_height = 0;
    std::string force_quit_program = "xkill";
    std::function<void()> reload;
    std::unique_ptr<XSettings> wm_xsettings;
    std::function<std::vector<XSetting>(Variant)> xsettings_source;
    std::vector<std::string> terminal_program = {"xterm"};
    std::string restart_command = Config().reboot_command;
    std::string shutdown_command = Config().poweroff_command;
};

} // namespace tiny::wm
