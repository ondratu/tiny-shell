#pragma once

#include <sys/types.h>

#include <chrono>
#include <functional>
#include <optional>
#include <vector>
#include <set>
#include <string>
#include <utility>

#include <X11/Xutil.h>
#include <X11/extensions/sync.h>

#include "wm/shell_theme.h"
#include "object.h"
#include "container.h"
#include "widget.h"
#include "display.h"
#include "wm/sync_gate.h"

namespace tiny::wm {

/**
 * @short Size of a client window that fits its WM_NORMAL_HINTS.
 *
 * The result is min..max (an unset limit does not count) and, when the client
 * asks for a resize increment, base + n * increment (ICCCM 4.1.2.3). Apply it
 * to the size of the client, not of its frame.
 *
 * `floor` is a minimum of the window manager (not of the client). It is kept
 * on the grid, but the maximum of the client wins over it.
 */
int fit_client_size(const XSizeHints& hints, bool horizontal, int size,
                    int floor = 0);

/**
 * @short Make a client size fit the aspect ratio of the hints (`PAspect`).
 *
 * The ratio (width - base) / (height - base) stays between `min_aspect` and
 * `max_aspect`. When it does not, `keep_width` tells which side gives way:
 * the height changes when it is set, the width otherwise. The limits of the
 * client (`PMinSize`, `PMaxSize`) win over the ratio.
 */
void fit_aspect(const XSizeHints& hints, int& width, int& height,
                bool keep_width);

/**
 * @short The frame after the edges `edges` (`tiny::Position` bits) are dragged
 * by dx, dy from `start`.
 *
 * `title` is the part of the frame that is not the client (its height), the
 * minimum is the one of the window manager for the whole frame. The edge
 * that is not dragged stays where it is, also when the size is limited.
 */
tiny::Rect resize_by_edges(const XSizeHints& hints, const tiny::Rect& start,
                           unsigned edges, int dx, int dy, int title,
                           int min_width, int min_height);

/// Number of increments (e.g. columns or rows of a terminal) in a client size.
int client_size_units(const XSizeHints& hints, bool horizontal, int size);

/// The client counts its size in units bigger than a pixel (a terminal).
bool has_resize_increment(const XSizeHints& hints);

struct MotifWMHints {
    unsigned long flags = 0;
    unsigned long functions = 0;
    unsigned long decorations = 0;
    long input_mode = 0;
    unsigned long status = 0;

    static constexpr long property_length = 5L;

    enum { // MWM flags
        FLAG_FUNCTIONS = 1L << 0,
        FLAG_DECORATIONS = 1L << 1,
        FLAG_INPUT_MODE = 1L << 2,
        FLAG_STATUS = 1L << 3
    };

    enum { // MWM functions
        FUNC_ALL = 1L << 0,
        FUNC_RESIZE = 1L << 1,
        FUNC_MOVE = 1L << 2,
        FUNC_MINIMIZE = 1L << 3,
        FUNC_MAXIMIZE = 1L << 4,
        FUNC_CLOSE = 1L << 5
    };

    enum { // MWM decorations
        DECOR_ALL = 1L << 0,
        DECOR_BORDER = 1L << 1,
        DECOR_RESIZE = 1L << 2,
        DECOR_TITLE = 1L << 3,
        DECOR_MENU = 1L << 4,
        DECOR_MINIMIZE = 1L << 5,
        DECOR_MAXIMIZE = 1L << 6
    };

    enum { // MWM input
        INPUT_MODELESS = 0,
        INPUT_APPLICATION_MODAL = 1,
        INPUT_SYSTEM_MODAL = 2,
        INPUT_FULL_APPLICATION_MODAL = 3
    };
};

/**
 * @short What a `_MOTIF_WM_HINTS` property allows.
 *
 * `data` are the `nitems` 32 bit values of the property (flags, functions,
 * decorations, ...); a property that is shorter than its flags say is read as
 * far as it goes. A field the hints do not set (or a missing property) allows
 * everything. When the `ALL` bit of a field is set, the other bits are the
 * ones that are NOT allowed (Motif); the result never has the `ALL` bit.
 */
void resolve_motif_hints(const unsigned long* data, unsigned long nitems,
                         unsigned long& functions, unsigned long& decorations);

//! A button press at the given root coordinates, to start a drag with
XEvent make_pointer_event(int x_root, int y_root);

//! Are two clicks (their server times in milliseconds) a double click?
bool is_double_click(bool has_last, unsigned long last, unsigned long time);

/**
 * @short Focus stealing prevention: may a new window take the focus?
 *
 * `time` is the `_NET_WM_USER_TIME` of the new window (`has_time` is false
 * when it has none), `last` the time of the last user action. A window
 * without the property is an old client and gets the focus; 0 means "not
 * after a user action" and never does. The times are server times of 32 bits,
 * compared with the wrap around.
 */
bool takes_focus(bool has_time, unsigned long time, bool has_last,
                 unsigned long last);

//! The width of the frame around a client on each side
struct FrameExtents {
    int left = 0;
    int right = 0;
    int top = 0;
    int bottom = 0;
};

//! The directions of a `_NET_WM_MOVERESIZE` request (EWMH)
enum MoveResizeDirection {
    SIZE_TOPLEFT = 0,
    SIZE_TOP = 1,
    SIZE_TOPRIGHT = 2,
    SIZE_RIGHT = 3,
    SIZE_BOTTOMRIGHT = 4,
    SIZE_BOTTOM = 5,
    SIZE_BOTTOMLEFT = 6,
    SIZE_LEFT = 7,
    MOVE_BY_POINTER = 8,
    SIZE_KEYBOARD = 9,
    MOVE_KEYBOARD = 10,
    MOVE_RESIZE_CANCEL = 11
};

/// The edges (`tiny::Position` bits) a sizing direction drags, 0 for the rest.
unsigned edges_of_direction(long direction);

/**
 * @short Where the top left corner of a frame goes, so the window lies where
 * its client asked for (ICCCM 4.1.2.3).
 *
 * The client asks for x, y as if it had no frame. Its `gravity` says which
 * point of the frame is placed there: a corner, the middle of an edge, the
 * center, or (`StaticGravity`) the top left corner of the client itself.
 */
std::pair<int, int> frame_position(int gravity, int x, int y,
                                   const FrameExtents& extents);

//! The area grown by the extents on each side
tiny::Rect grow_by(const tiny::Rect& area, const FrameExtents& extents);

/// How far the frame moves, when the client grows by dw, dh and keeps its
/// reference point (the gravity) where it was.
std::pair<int, int> gravity_shift(int gravity, int dw, int dh);

/// The gravity of the hints, `NorthWestGravity` when they have none.
int window_gravity(const XSizeHints& hints);

class Window {
  public:
    enum class WMType {
        DESKTOP,
        DOCK,
        TOOLBAR,
        MENU,
        UTILITY,
        SPLASH,
        DIALOG,
        //! A notification, a menu (popup, dropdown), a tooltip, the list of a
        //! combo box or the picture of a drag: shown where the client puts it,
        //! with no frame, no focus, above the windows; the shell does not
        //! manage it
        POPUP,
        NORMAL
    };

    enum WMState {
        MODAL = 1L << 0,
        STICKY = 1L << 1,
        MAXIMIZED_VERT = 1L << 2,
        MAXIMIZED_HORZ = 1L << 3,
        SHADED = 1L << 4,
        SKIP_TASKBAR = 1L << 5,
        SKIP_PAGER = 1L << 6,
        HIDDEN = 1L << 7,
        FULLSCREEN = 1L << 8,
        ABOVE = 1L << 9,
        BELOW = 1L << 10,
        DEMANDS_ATTENTION = 1L << 11,
        FOCUSED = 1L << 12
    };

    Window(::Window child, ::Window root, unsigned long functions,
           const tiny::Rect& area);
    virtual ~Window();

    static Window* create(::Window root, ::Window child,
                          const XWindowAttributes& attrs,
                          unsigned long functions, const tiny::Rect& area);

    //! The areas of the monitor that a window is on
    struct MonitorAreas {
        tiny::Rect work;   //!< without the panel and the docks
        tiny::Rect screen; //!< the whole monitor, where a fullscreen window is
    };

    //! Tells the areas of the monitor that has most of a frame
    using AreaFinder = std::function<MonitorAreas(const tiny::Rect& frame)>;

    //! Where maximized windows lie; a maximized window moves there at once
    void set_work_area(const tiny::Rect& area);

    /**
     * @short Let the window find its monitor.
     *
     * The window asks for the areas of the monitor it is on when it is
     * maximized, made fullscreen or restored, and when it asks for a place.
     * Without a finder the work area that was set is used, and a fullscreen
     * window covers the root window.
     */
    void set_area_finder(AreaFinder finder) { find_areas = std::move(finder); }

    /**
     * @short `_NET_WM_FULLSCREEN_MONITORS`: the area that the fullscreen
     * window covers instead of one monitor. A window that is fullscreen
     * moves there at once, the others when they go fullscreen. It is
     * dropped when the window leaves fullscreen or `drop_fullscreen_span()`.
     */
    void set_fullscreen_span(const tiny::Rect& area);

    //! The window covers the monitor it is on again
    void drop_fullscreen_span();

    //! Is the area of `set_fullscreen_span()` in use
    bool has_fullscreen_span() const { return span.width > 0; }

    //! Look for the monitor of the window again (a monitor, a panel or a dock
    //! came or went): a maximized or fullscreen window follows its monitor
    void refresh_areas();

    ::Window get_child() const { return child; }

    virtual bool is_resizable() const
    {
        return functions & MotifWMHints::FUNC_RESIZE;
    }

    virtual bool is_maximizable() const
    {
        return functions & MotifWMHints::FUNC_MAXIMIZE;
    }

    //! The user may minimize it (the Motif function, and not a small helper
    //! window that only goes with its application)
    virtual bool is_minimizable() const
    {
        return (functions & MotifWMHints::FUNC_MINIMIZE) && !is_auxiliary();
    }

    //! The user may move it (the Motif function)
    bool is_movable() const { return functions & MotifWMHints::FUNC_MOVE; }

    //! The user may close it with a button (the Motif function)
    virtual bool is_closable() const
    {
        return functions & MotifWMHints::FUNC_CLOSE;
    }

    //! A dialog, toolbar, menu or utility window: no place of its own in the
    //! dock, no minimize and maximize buttons
    bool is_auxiliary() const
    {
        return wm_type == WMType::DIALOG || wm_type == WMType::UTILITY ||
               wm_type == WMType::TOOLBAR || wm_type == WMType::MENU;
    }

    //! Not in the dock: asked by `_NET_WM_STATE_SKIP_TASKBAR` or a utility
    //! window, a toolbar or a menu
    virtual bool skip_taskbar() const
    {
        return (wm_states & WMState::SKIP_TASKBAR) ||
               wm_type == WMType::UTILITY || wm_type == WMType::TOOLBAR ||
               wm_type == WMType::MENU;
    }
    void set_skip_taskbar(bool skip);

    //! `_NET_WM_STATE_ABOVE` and `_NET_WM_STATE_BELOW`: the stacking layer
    bool is_above() const { return wm_states & WMState::ABOVE; }
    bool is_below() const { return wm_states & WMState::BELOW; }
    void set_above(bool above);
    void set_below(bool below);

    //! `_NET_WM_STATE_DEMANDS_ATTENTION`, cleared when the window is focused
    bool demands_attention() const
    {
        return wm_states & WMState::DEMANDS_ATTENTION;
    }
    void set_demands_attention(bool demands);

    virtual bool skip_pager() const { return wm_states & WMState::SKIP_PAGER; }
    void set_skip_pager(bool skip);

    //! `_NET_WM_STATE_STICKY`: on all desktops. There is one desktop, so it
    //! only changes `_NET_WM_DESKTOP` (0xFFFFFFFF, like a pager expects)
    bool is_sticky() const { return wm_states & WMState::STICKY; }
    void set_sticky(bool sticky);

    //! Value of `_NET_WM_DESKTOP` meaning all the desktops
    static constexpr unsigned long all_desktops = 0xFFFFFFFFUL;

    //! Write `_NET_WM_DESKTOP` of the client
    void publish_desktop();

    //! The user may roll the window up to its title bar
    virtual bool is_shadable() const { return false; }

    //! The `_NET_WM_ACTION_*` atoms of what the user may do with the window
    std::vector<Atom> get_allowed_actions() const;

    //! Write `_NET_WM_ALLOWED_ACTIONS` of the client
    void publish_allowed_actions();

    //! A modal dialog keeps the focus away from its parent
    virtual bool is_modal() const { return wm_states & WMState::MODAL; }
    void set_modal(bool modal);

    virtual bool get_minimized() const { return wm_states & WMState::HIDDEN; }
    void set_minimized(bool minimized);

    virtual bool get_maximized() const
    {
        return (wm_states & WMState::MAXIMIZED_VERT &&
                wm_states & WMState::MAXIMIZED_HORZ);
    }
    void set_maximized(bool maximized);
    void set_fullscreen(bool fullscreen);

    bool is_shaded() const { return wm_states & WMState::SHADED; }

    //! Roll the window up to its title bar and back (only a frame can)
    virtual void shade(bool) {}

    void set_shaded(bool shaded);

    virtual bool is_fullscreen() const
    {
        return wm_states & WMState::FULLSCREEN;
    }

    virtual bool is_focused() const { return wm_states & WMState::FOCUSED; }

    void get_wm_states(std::vector<Atom>& atoms);
    void publish_wm_states();

    const std::string& get_wm_name() const { return wm_name; }

    const std::string& get_wm_icon_name() const { return wm_icon_name; }

    /* @short Return icon scaled to shell_theme.icon size.
         *
         * Pixmap will be destroyed in Window destructor if it set, otherwise
         * zero is returned.
         * */
    Pixmap get_icon() const { return icon; }

    //! The icon of the application (not owned), preferred to `get_icon()`
    struct AppIcon {
        Picture picture = None;
        int width = 0;
        int height = 0;
    };
    const AppIcon& get_app_icon() const { return app_icon; }
    void set_app_icon(const AppIcon& value) { app_icon = value; }

    //! The desktop file id of the application the window belongs to, empty
    //! when none is known
    const std::string& get_app_id() const { return app_id; }
    void set_app_id(std::string value) { app_id = std::move(value); }

    //! WM_CLASS of the client, empty when it has none
    const std::string& get_wm_class_instance() const { return wm_class[0]; }
    const std::string& get_wm_class() const { return wm_class[1]; }
    Pixmap get_icon_mask() const { return icon_mask; }

    virtual void set_focus();
    virtual void return_focus();

    WMType get_wm_type() const { return wm_type; }

    //! The window the client is a dialog of (`WM_TRANSIENT_FOR`), or None
    ::Window get_transient_for() const { return transient_for; }
    //! The group of the window (`WM_HINTS`), or None
    ::Window get_window_group() const { return window_group; }

    //! The top level window the manager shows: the frame, or the client
    virtual ::Window frame() const { return child; }

    //! Put the frame above its siblings
    //! The theme changed its colors: repaint the decorations
    virtual void restyle_frame() {}

    virtual void raise();

    //! Put the frame under its siblings
    virtual void lower();

    //! Put the frame right under `sibling` (a top level window)
    virtual void stack_under(::Window sibling);

    //! Put the frame right over `sibling` (a top level window)
    virtual void stack_over(::Window sibling);

    //! The frame around the client (nothing for a window without a frame)
    virtual FrameExtents frame_extents() const { return {}; }

    //! Write `_NET_FRAME_EXTENTS` of the client
    void publish_frame_extents();

    //! Move the frame, the root coordinates of its top left corner
    virtual void move_to(int x, int y);

    //! Send WM_DELETE_WINDOW to window or xkill that; a window that does
    //! not answer `_NET_WM_PING` is not killed at once: `on_unresponsive`
    //! asks the user first
    virtual void close();

    /**
     * `_NET_WM_SYNC_REQUEST`: the client that has a counter (and says so in
     * `WM_PROTOCOLS`) is asked to draw each size of an interactive resize and
     * is not given the next one before it has. True when there is no counter or
     * the client has answered the last request (or does not answer in time).
     */
    bool sync_allows_resize();

    //! Ask the client to draw the size that follows; does nothing without a
    //! counter
    void sync_request_resize();

    //! The next resize starts: nothing is waited for
    void sync_reset() { sync_gate.reset(); }

    //! The counter of the client, None when it has none or does not use it
    XSyncCounter get_sync_counter() const { return sync_counter; }

    //! Grab Alt + F4 of the client on the key that has F4 now (the layout
    //! can change); the key of the last grab is released
    void grab_close_key();
    //! Send `WM_DELETE_WINDOW`; false (nothing sent) without that protocol
    bool ask_to_close();

    using Clock = std::chrono::steady_clock;

    //! How long a client may take to answer `_NET_WM_PING`
    static constexpr std::chrono::seconds ping_timeout{5};

    //! How often a client that is not minimized is pinged
    static constexpr std::chrono::seconds ping_interval{5};

    //! How long a client that does not respond is left alone, before the
    //! user is asked what to do with it
    static constexpr std::chrono::seconds ask_delay{10};

    //! How long the user is left alone after the choice to wait
    static constexpr std::chrono::seconds wait_delay{30};

    //! Ask the client whether it lives (`_NET_WM_PING`); nothing is sent
    //! without that protocol or while the last ping is not answered
    void ping(Clock::time_point now);

    //! The client answered the ping: it responds again
    void pong();

    //! False when the client did not answer a ping in time
    bool is_responding() const { return responding; }

    //! A ping waits for the answer
    bool has_pending_ping() const { return ping_pending; }

    //! When the pending ping is overdue
    Clock::time_point get_ping_deadline() const { return ping_deadline; }

    //! Send the next ping when its time has come (see `ping_interval`)
    void ping_if_due(Clock::time_point now);

    //! When `expire_ping` or `ping_if_due` has something to do; none for a
    //! client that is not pinged
    std::optional<Clock::time_point> get_ping_wakeup() const;

    //! When to ask the user about a client that does not respond; none while
    //! the client responds or is minimized
    std::optional<Clock::time_point> get_ask_time() const;

    //! The user chose to wait: ask again after `wait_delay`
    void postpone_ask(Clock::time_point now);

    //! The process of the client that may be killed (see `killable_pid`),
    //! 0 if there is none
    pid_t get_killable_pid() const;

    //! The process of the client is known and does not exist any more
    bool client_process_gone() const;

    //! Kill the process of the client, when there is one to kill, and its
    //! connection to the X server
    void kill_client();

    //! Give up waiting for an overdue ping; true when it just happened
    bool expire_ping(Clock::time_point now);

    virtual void minimize();
    virtual void maximize();

    /*!
     * An UnmapNotify of the client came; true when it is the echo of an unmap
     * the manager did itself (ICCCM 4.1.4), false when the client withdrew.
     * Only a window without a frame hides its client; a frame is unmapped
     * with the client inside, which sends no event.
     */
    bool consume_unmap();

    virtual void restore(int x = 0, int y = 0);

    //! A window mapped with the state maximized or fullscreen is put there
    void apply_initial_state();

    //! Cover the whole screen (the panel too) without decorations, or go back
    virtual void fullscreen(bool on);

    //! Write WM_STATE (NormalState, IconicState, WithdrawnState) of the client
    void set_wm_state(long wm_state);

    //! The client leaves the manager: WithdrawnState and no _NET_WM_STATE
    void withdraw();

    //! The size a client may have: the hints of the client applied to what
    //! it asked for (`CWWidth`, `CWHeight` of `mask`)
    void fit_requested_size(unsigned long mask, int& width, int& height) const;

    //! The client asks for another position or size (ConfigureRequest)
    virtual void configure_request(const XConfigureRequestEvent& e);

    /**
     * @short `_NET_MOVERESIZE_WINDOW`: the client asks for a place and a size
     * as if it had no frame.
     *
     * `data[0]` of the message has the gravity (0: the one of the window), the
     * flags of the values that are given (x, y, width, height) and the source;
     * the position is where the reference point of the gravity goes.
     */
    void move_resize_request(const XClientMessageEvent& e);

    //! Tell the client where it is, in the root coordinates (ICCCM 4.1.5)
    void send_configure_notify();

    void update_protocols();         //!< Update WMProtocols
    void update_sync_counter();      //!< `_NET_WM_SYNC_REQUEST_COUNTER`
    void update_properties();        //!< Update WMProperties
    void update_normal_hints();      //!< Update XSizeHints;
    void update_wm_hints();          //!< Update XWMHints;
    void update_wm_states();         //!< Update WMStates
    void update_gtk_frame_extents(); //!< `_GTK_FRAME_EXTENTS`
    void update_desktop();      //!< `_NET_WM_DESKTOP` of all desktops: sticky
    void update_wm_name();      //!< Update wm_name
    void update_wm_class();     //!< Update wm_class
    void update_wm_icon_name(); //!< Update wm_icon_name
    void update_wm_icon();
    void update_transient_for(); //!< Update transient_for
    void set_events();

    //! Get and fill window properties
    static int get_properties(::Window window, std::set<Atom>& properties);

    //! Return _NET_WM_TYPE or WMType::NORMAL when property can't be read
    static WMType get_net_wm_type(::Window window);

    /**
     * `_GTK_FRAME_EXTENTS` of the window: the invisible border (a shadow) that
     * a GTK application with client-side decorations draws around its window.
     * Zeros when there is none. The window is placed so the visible part, not
     * the border, lies in the work area.
     */
    static FrameExtents get_gtk_frame_extents(::Window window);

    //! `_NET_WM_USER_TIME` of the window, or of the window its
    //! `_NET_WM_USER_TIME_WINDOW` names; false when there is none
    static bool get_user_time(::Window window, unsigned long& time);

    //! Return _MOTIF_WM_HINTS od return ALL flags set
    static bool get_motif_hints(::Window window, unsigned long& functions,
                                unsigned long& decorations);

    /**
     * @short Move (no edges) or resize the window with the pointer.
     *
     * Like the drag of the title bar or an edge, but started and driven by
     * the manager (`_NET_WM_MOVERESIZE`): the pointer is at x_root, y_root.
     */
    virtual void begin_drag(unsigned edges, int x_root, int y_root);

    //! The pointer moved while the window is dragged
    virtual void drag_to(int x_root, int y_root);

    virtual void end_drag();

    virtual void on_window_drag_begin(tiny::Object* o, const XEvent& e,
                                      void* data);

    virtual void on_window_drag_motion(tiny::Object* o, const XEvent& e,
                                       void* data);

    tiny::Signal on_focus;
    //! The window is minimized or restored, see `get_minimized()`
    tiny::Signal on_minimize;
    tiny::Signal on_modal; //!< The modal state changed
    //! `skip_taskbar()` may have changed (the dock entry comes or goes)
    tiny::Signal on_skip_taskbar;
    //! The layer (above, below) changed; the manager restacks the windows
    tiny::Signal on_layer;
    //! `demands_attention()` changed (the dock marks the icon)
    tiny::Signal on_attention;
    //! The client wrote `_NET_WM_USER_TIME` (with every key press)
    tiny::Signal on_user_time;
    //! The frame comes or goes (the decorations are none, or were none)
    tiny::Signal on_reframe;
    //! The client asks to be moved or resized (`_NET_WM_MOVERESIZE`), the
    //! event is the message
    tiny::Signal on_move_resize_request;
    //! `close()` was asked of a client that does not respond
    tiny::Signal on_unresponsive;
    tiny::Signal on_drag_begin;
    tiny::Signal on_drag_motion;
    //! The window is dragged by its body with Alt or Super held
    tiny::Signal on_modifier_drag;

  protected:
    //! Handlers of the events of the client window, removed with the window
    tiny::EventConnections window_events;

    KeyCode close_key = 0; //!< the key grabbed for Alt + F4

    /* event handlers */
    virtual void on_client_message(const XEvent& e, void* data);

    //! A `_NET_WM_STATE` request: each of the two atoms on its own
    void on_net_wm_state(const XClientMessageEvent& e);

    virtual void on_property_notify(const XEvent& e, void* data);

    //! `_NET_WM_USER_TIME` changed on the window of `_NET_WM_USER_TIME_WINDOW`
    void on_user_time_window_notify(const XEvent& e, void* data);

    //! Listen to the window of `_NET_WM_USER_TIME_WINDOW` (none: the client)
    void watch_user_time_window();

    //! `is_responding()` changed
    virtual void on_responding_changed() {}

    //! `_MOTIF_WM_HINTS` changed; `functions` is new, the decorations are not
    virtual void on_motif_hints_changed(unsigned long new_decorations);

    virtual void on_button_press(const XEvent& e, void* data);

    virtual void on_button_release(const XEvent& e, void* data);

    virtual void on_key_release(const XEvent& e, void* data);

    void on_motion_notify(const XEvent& e, void* data);

    /**
     * @short Is the FocusIn/FocusOut event a change of the keyboard focus?
     *
     * Not when it only reports a grab of the keyboard (`NotifyGrab`,
     * `NotifyUngrab`, e.g. the Alt + Tab grab of the window manager) or the
     * window under the pointer (`NotifyPointer`, `NotifyPointerRoot`,
     * `NotifyDetailNone`, `NotifyInferior`): the window under the pointer
     * would be taken as the focused one. A subclass that reacts to the focus
     * (the header of `Client`) checks it before its own change; the base
     * class checks it again before it emits `on_focus`.
     */
    static bool is_focus_change(const XEvent& e);

    virtual void on_focus_in(const XEvent& e, void* data);

    virtual void on_focus_out(const XEvent& e, void* data);

    //! Cover the work area (a maximized window)
    virtual void place_maximized();

    //! Cover the monitor (a fullscreen window)
    virtual void place_fullscreen();

    //! The frame of the window on the root window
    virtual tiny::Rect frame_geometry() const;

    //! Use the areas of the monitor that has most of the frame; nothing
    //! changes without a finder
    void use_monitor_of(const tiny::Rect& frame);

    //! The monitor of a fullscreen window: the root window without a finder
    tiny::Rect fullscreen_area() const;

    //! A dock that came later does not cover a normal window
    virtual void keep_in_work_area();

    //! The size to restore to of a window that starts maximized; `title` is
    //! the height of the frame above the client
    void init_restore_size(int title);

    ::Window child;
    ::Window root;
    tiny::Display& dsp;

    XSizeHints* hints;
    tiny::Rect state = {0, 0, 0, 0};
    tiny::Rect before_fullscreen = {0, 0, 0, 0};
    tiny::Rect work_area = {0, 0, 0, 0};
    tiny::Rect screen_area = {0, 0, 0, 0}; //!< empty: the root window
    tiny::Rect span = {0, 0, 0, 0};        //!< empty: the monitor of the window
    AreaFinder find_areas;

    FrameExtents gtk_extents; //!< see `get_gtk_frame_extents()`
    unsigned long functions = 0;
    unsigned long wm_states = 0;
    ::Window transient_for = None;
    ::Window window_group = None;
    ::Window user_time_window = None; //!< listened to, None: the client
    WMType wm_type = WMType::NORMAL;
    std::string wm_name;
    bool is_net_wm_name = true;
    std::string wm_icon_name;

    Pixmap icon = 0;
    AppIcon app_icon;
    std::string app_id;
    std::string wm_class[2]; //!< instance, class
    Pixmap icon_mask = 0;

    XEvent start_event; // state before moving/resizing
    XWindowAttributes start_attrs;

    std::set<Atom> protocols;
    XSyncCounter sync_counter = None;
    SyncGate sync_gate;
    std::set<Atom> properties;

    unsigned drag_edges = 0; //!< of `begin_drag`, none: the window is moved

    bool responding = true;
    unsigned expected_unmaps = 0; //!< our unmaps of the client not seen yet
    bool ping_pending = false;
    Clock::time_point ping_deadline;
    Clock::time_point next_ping;
    Clock::time_point ask_at; //!< valid while the client does not respond
};

} // namespace tiny::wm
