#pragma once

#include <vector>
#include <set>
#include <string>

#include "object.h"
#include "widget.h"
#include "display.h"

namespace wm {

struct MotifWMHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;

    static constexpr long property_length = 5L;

    enum { // MWM flags
        FLAG_FUNCTIONS     = 1L<<0,
        FLAG_DECORATIONS   = 1L<<1,
        FLAG_INPUT_MODE    = 1L<<2,
        FLAG_STATUS        = 1L<<3
    };

    enum { // MWM functions
        FUNC_ALL        = 1L<<0,
        FUNC_RESIZE     = 1L<<1,
        FUNC_MOVE       = 1L<<2,
        FUNC_MINIMIZE   = 1L<<3,
        FUNC_MAXIMIZE   = 1L<<4,
        FUNC_CLOSE      = 1L<<5
    };

    enum { // MWM decorations
        DECOR_ALL       = 1L<<0,
        DECOR_BORDER    = 1L<<1,
        DECOR_RESIZE    = 1L<<2,
        DECOR_TITLE     = 1L<<3,
        DECOR_MENU      = 1L<<4,
        DECOR_MINIMIZE  = 1L<<5,
        DECOR_MAXIMIZE  = 1L<<6
    };

    enum { // MWM input
        INPUT_MODELESS                  = 0,
        INPUT_APPLICATION_MODAL         = 1,
        INPUT_SYSTEM_MODAL              = 2,
        INPUT_FULL_APPLICATION_MODAL    = 3
    };
};


class Window: public tiny::Object {
    public:
        enum class WMType {
            DESKTOP,
            DOCK,
            TOOLBAR,
            MENU,
            UTILITY,
            SPLASH,
            DIALOG,
            NORMAL
        };

        enum WMState {
            MODAL               = 1L<<0,
            STICKY              = 1L<<1,
            MAXIMIZED_VERT      = 1L<<2,
            MAXIMIZED_HORZ      = 1L<<3,
            SHADED              = 1L<<4,
            SKIP_TASKBAR        = 1L<<5,
            SKIP_PAGER          = 1L<<6,
            HIDDEN              = 1L<<7,
            FULLSCREEN          = 1L<<8,
            ABOVE               = 1L<<9,
            BELOW               = 1L<<10,
            DEMANDS_ATTENTION   = 1L<<11,
            FOCUSED             = 1L<<12
        };

    public:
        Window(::Window child, ::Window root, unsigned long functions);
        virtual ~Window();

        static Window* create(::Window root, ::Window child,
                const XWindowAttributes& attrs, unsigned long functions);

        inline ::Window get_child() const
        { return child; }

        virtual inline bool is_resizable() const
        { return functions & MotifWMHints::FUNC_RESIZE; }

        virtual inline bool is_maximizable() const
        { return functions & MotifWMHints::FUNC_MAXIMIZE; }

        virtual inline bool skip_taskbar() const
        { return wm_states & WMState::SKIP_TASKBAR; }

        virtual inline bool skip_pager() const
        { return wm_states & WMState::SKIP_PAGER; }

        virtual inline bool get_minimized() const
        { return wm_states & WMState::HIDDEN; }
        void set_minimized(bool minimized);

        virtual inline bool get_maximized() const
        { return (wm_states & WMState::MAXIMIZED_VERT &&
                  wm_states & WMState::MAXIMIZED_HORZ); }
        void set_maximized(bool maximized);

        virtual inline bool is_fullscreen() const
        { return wm_states & WMState::FULLSCREEN; }

        virtual inline bool is_focused() const
        { return wm_states & WMState::FOCUSED; }

        void get_wm_states(std::vector<Atom>& atoms);

        inline std::string get_wm_name() const
        { return wm_name; }

        std::string get_wm_icon_name() const
        { return wm_icon_name; }

        /* @short Return icon scaled to tiny::theme.wm_icon size.
         *
         * Pixmap will be destroyed in Window destructor if it set, otherwise
         * zero is returned.
         * */
        inline Pixmap get_icon() const
        { return icon; }
        inline Pixmap get_icon_mask() const
        { return icon_mask; }

        virtual void set_focus();
        virtual void return_focus();

        //! Send WM_DELETE_WINDOW to window or xkill that
        virtual void close();

        virtual void minimize();
        virtual void maximize();

        virtual void restore(int x=0, int y=0);

        void update_protocols();        //!< Update WMProtocols
        void update_properties();       //!< Update WMProperties
        void update_normal_hints();     //!< Update XSizeHints;
        void update_wm_hints();         //!< Update XWMHints;
        void update_wm_states();        //!< Update WMStates
        void update_wm_name();          //!< Update wm_name
        void update_wm_icon_name();     //!< Update wm_icon_name
        void update_wm_icon();
        void set_events();

        //! Get and fill window properties
        static int get_properties(::Window window,
                std::set<Atom>& properties);

        //! Return _NET_WM_TYPE or WMType::NORMAL when property can't be read
        static WMType get_net_wm_type(::Window window);

        //! Return _MOTIF_WM_HINTS od return ALL flags set
        static bool get_motif_hints(::Window window,
                unsigned long& functions, unsigned long& decorations);

        virtual void on_window_drag_begin(tiny::Object *o, const XEvent &e,
                void * data);

        virtual void on_window_drag_motion(tiny::Object *o, const XEvent &e,
                void * data);

        tiny::Signal on_focus;
        tiny::Signal on_drag_begin;
        tiny::Signal on_drag_motion;
    protected:
        /* event handlers */
        virtual void on_client_message(const XEvent &e, void *data);

        virtual void on_property_notify(const XEvent &e, void *data);

        virtual void on_button_press(const XEvent &e, void *data);

        virtual void on_button_release(const XEvent &e, void *data);

        virtual void on_key_release(const XEvent &e, void* data);

        void on_motion_notify(const XEvent &e, void *data);

        virtual void on_focus_in(const XEvent &e, void *data);

        virtual void on_focus_out(const XEvent &e, void *data);

        ::Window child;
        ::Window root;
        tiny::Display& dsp;

        XSizeHints* hints;
        tiny::Rect state = {0, 0, 1, 1};

        unsigned long functions = 0;
        unsigned long wm_states = 0;
        std::string wm_name;
        bool is_net_wm_name = true;
        std::string wm_icon_name;

        Pixmap icon = 0;
        Pixmap icon_mask = 0;

        XEvent start_event;                 // state before moving/resizing
        XWindowAttributes start_attrs;

        std::set<Atom> protocols;
        std::set<Atom> properties;
    };

} // namespace wm
