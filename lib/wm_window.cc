#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>

#include "wm_window.h"
#include "x_util.h"

namespace wm {

Window::Window(::Window child, ::Window root, unsigned long functions):
    child(child),
    root(root),
    dsp(tiny::get_display()),
    functions(functions)
{
    hints = XAllocSizeHints();
    if (hints == nullptr) {
        throw std::bad_alloc();
    }

    update_protocols();
    update_properties();

    if (properties.count(dsp._NET_WM_STATE)){
        update_wm_states();
    }
    if (properties.count(dsp.WM_NORMAL_HINTS)){
        update_normal_hints();
    }

    if (get_maximized() && (state.width == 0 || state.height == 0)){
        if (hints->flags & PMinSize) {
            state.width = hints->min_width;
            state.height = hints->min_height;
        }

        XWindowAttributes attrs;
        XGetWindowAttributes(dsp, root, &attrs);  //!< TODO: get screen size instead
        state.width = std::max(state.width, (uint32_t)attrs.width/3*2);
        state.height = std::max(state.height, (uint32_t)attrs.height/3*2);

        if (hints->flags & PMaxSize) {
            state.width = std::min(state.width, (uint32_t)hints->max_width);
            state.height = std::min(state.height, (uint32_t)hints->max_height);
        }
        if (hints->flags & PAspect){
            // TODO: fix ratio if is set
            TINY_LOG("Aspect ratio problem...");
        }
    }
}

Window::~Window()
{
    if (hints){
        XFree(hints);
    }
}

Window* Window::create(::Window root, ::Window child,
                       const XWindowAttributes& attrs,
                       unsigned long functions)
{
    tiny::Display& display = tiny::get_display();
    auto win = new Window(child, root, functions);

    XMapWindow(display, child);
    XSetWindowBorderWidth(display, child, 1);
    XRaiseWindow(display, child);

    win->set_events();
    // TODO: move down under panel
    return win;
}

void Window::set_minimized(bool minimized)
{
    if (minimized){
        wm_states |= WMState::HIDDEN;
    } else {
        wm_states &= ~(WMState::HIDDEN);
    }

    std::vector<Atom> atoms;
    get_wm_states(atoms);

    XChangeProperty(dsp, child, dsp._NET_WM_STATE, XA_ATOM,
            32, PropModeReplace,
            reinterpret_cast<unsigned char *>(&(*atoms.begin())),
            atoms.size());
}

void Window::set_maximized(bool maximized)
{
    if (maximized){
        wm_states |= WMState::MAXIMIZED_VERT;
        wm_states |= WMState::MAXIMIZED_HORZ;
    } else {
        wm_states &= ~(WMState::MAXIMIZED_VERT);
        wm_states &= ~(WMState::MAXIMIZED_HORZ);
    }

    std::vector<Atom> atoms;
    get_wm_states(atoms);

    XChangeProperty(dsp, child, dsp._NET_WM_STATE, XA_ATOM,
            32, PropModeReplace,
            reinterpret_cast<unsigned char *>(&(*atoms.begin())),
            atoms.size());
}

void Window::get_wm_states(std::vector<Atom>& atoms)
{
    atoms.clear();

    if (wm_states & WMState::MODAL){
        atoms.push_back(dsp._NET_WM_STATE_MODAL); }
    if (wm_states & WMState::STICKY){
        atoms.push_back(dsp._NET_WM_STATE_STICKY); }
    if (wm_states & WMState::MAXIMIZED_VERT){
        atoms.push_back(dsp._NET_WM_STATE_MAXIMIZED_VERT); }
    if (wm_states & WMState::MAXIMIZED_HORZ){
        atoms.push_back(dsp._NET_WM_STATE_MAXIMIZED_HORZ); }
    if (wm_states & WMState::SHADED){
        atoms.push_back(dsp._NET_WM_STATE_SHADED); }
    if (wm_states & WMState::SKIP_TASKBAR){
        atoms.push_back(dsp._NET_WM_STATE_SKIP_TASKBAR); }
    if (wm_states & WMState::SKIP_PAGER){
        atoms.push_back(dsp._NET_WM_STATE_SKIP_PAGER); }
    if (wm_states & WMState::HIDDEN){
        atoms.push_back(dsp._NET_WM_STATE_HIDDEN); }
    if (wm_states & WMState::FULLSCREEN){
        atoms.push_back(dsp._NET_WM_STATE_FULLSCREEN); }
    if (wm_states & WMState::ABOVE){
        atoms.push_back(dsp._NET_WM_STATE_ABOVE); }
    if (wm_states & WMState::BELOW){
        atoms.push_back(dsp._NET_WM_STATE_BELOW); }
    if (wm_states & WMState::DEMANDS_ATTENTION){
        atoms.push_back(dsp._NET_WM_STATE_DEMANDS_ATTENTION); }
    if (wm_states & WMState::FOCUSED){
        atoms.push_back(dsp._NET_WM_STATE_FOCUSED); }
}

void Window::set_focus()
{
    XSetInputFocus(dsp, child, RevertToPointerRoot, CurrentTime);

    if (protocols.count(dsp.WM_TAKE_FOCUS))
    {
        XClientMessageEvent msg;
        msg.message_type = dsp.WM_PROTOCOLS;
        msg.display = dsp;
        msg.window = child;
        msg.format = 32;
        msg.data.l[0] = dsp.WM_TAKE_FOCUS;
        msg.data.l[1] = CurrentTime;

        XSendEvent(dsp, child, false, NoEventMask, (XEvent*) &msg);
    }
    XRaiseWindow(dsp, child);
}

void Window::close()
{
    if (protocols.count(dsp.WM_DELETE_WINDOW))
    {
        XClientMessageEvent msg;
        msg.window = child;
        msg.type = ClientMessage;
        msg.format = 32;
        msg.message_type = dsp.WM_PROTOCOLS;
        msg.data.l[0] = dsp.WM_DELETE_WINDOW;
        XSendEvent(dsp, child, false, NoEventMask, (XEvent*) &msg);
    } else {
        XKillClient(dsp, child);
    }
}

void Window::maximize()
{
    if (get_maximized()){
        return;
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(dsp, child, &attrs);
    state.x = attrs.x;
    state.y = attrs.y;
    state.width = attrs.width;
    state.height = attrs.height;

    XGetWindowAttributes(dsp, root, &attrs);  //!< TODO: get screen size instead
                                              // TODO: move under the panel
    XMoveResizeWindow(dsp, child, 0, 0, attrs.width, attrs.height);
    set_maximized(true);
}

void Window::restore(int x, int y)
{
    if (get_maximized()) {
        // TODO: stay in desktop
        x = x ? (x - state.width/2): state.x;
        y = y ? (y - state.height/2) : state.y;

        XMoveResizeWindow(dsp, child,
                x, y, state.width, state.height);
        XSetWindowBorderWidth(dsp, child, 1);
        set_maximized(false);
    } else {
        XMapWindow(dsp, child);
        set_minimized(false);
    }
}

void Window::update_protocols()
{
    protocols.clear();

    Atom* supported;
    int count;
    if (XGetWMProtocols(dsp, child, &supported, &count) == 0) {
        return;
    }
    for (size_t i =0; i < size_t(count); ++i){
        protocols.insert(supported[i]);
    }
    XFree(supported);
}

void Window::update_properties()
{
    properties.clear();

    int count;
    Atom * props = XListProperties(dsp, child, &count);

    for (int i=0; i < count; ++i){
        properties.insert(props[i]);
    }
    XFree(props);
}

void Window::update_normal_hints()
{
    /*
     * USPosition|USSize|PPosition|PSize|PMinSize|PMaxSize|PResizeInc|PAspect
     * PBaseSize|PWinGravity
     */
    long supplied;
    XGetWMNormalHints(dsp, child, hints, &supplied);
    // Min and Max size are not set
    if (!(supplied & PMinSize) && !(supplied & PMaxSize))
    {
        //? Really
        functions &= ~(MotifWMHints::FUNC_ALL);
        functions &= ~(MotifWMHints::FUNC_RESIZE);
        functions &= ~(MotifWMHints::FUNC_MAXIMIZE);
    }
    // Min and Max are set but are same
    if (supplied & PMinSize && supplied &PMaxSize &&
            (hints->min_width == hints->max_width) &&
            (hints->min_height == hints->max_height))
    {
        functions &= ~(MotifWMHints::FUNC_ALL);
        functions &= ~(MotifWMHints::FUNC_RESIZE);
        functions &= ~(MotifWMHints::FUNC_MAXIMIZE);
    }

    if (supplied & PBaseSize) {
        state.width = hints->base_width;
        state.height = hints->base_height;
    }
}

void Window::update_wm_states()
{
    wm_states = 0L;

    Atom returned_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char *data = NULL;

    if (XGetWindowProperty(dsp, child,
                dsp._NET_WM_STATE, 0L, 1L,
                false, XA_ATOM,
                &returned_type, &size,
                &nitems, &bytes_left, &data) != Success || !nitems)
    {
        // No _NET_WM_STATE
        return;
    }

    if (bytes_left != 0) {  // Fetch all states...
        XFree(data);
        unsigned long remain = ((size / 8) * nitems) + bytes_left;
        if (XGetWindowProperty(dsp, child, dsp._NET_WM_STATE,
                    0L, remain, false, XA_ATOM,
                    &returned_type, &size,
                    &nitems, &bytes_left, &data) != Success)
        {
            // No _NET_WM_STATE
            return;
        }
    }

    char* atom_name;
    Atom* atoms = reinterpret_cast<Atom*>(data);
    for (unsigned long i = 0; i < nitems; i++){
        if (atoms[i] == dsp._NET_WM_STATE_MODAL){
            wm_states |= WMState::MODAL;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_STICKY){
            wm_states |= WMState::STICKY;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_MAXIMIZED_VERT){
            wm_states |= WMState::MAXIMIZED_VERT;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_MAXIMIZED_HORZ){
            wm_states |= WMState::MAXIMIZED_HORZ;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_SHADED){
            wm_states |= WMState::SHADED;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_SKIP_TASKBAR){
            wm_states |= WMState::SKIP_TASKBAR;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_SKIP_PAGER){
            wm_states |= WMState::SKIP_PAGER;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_HIDDEN){
            wm_states |= WMState::HIDDEN;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_FULLSCREEN){
            wm_states |= WMState::FULLSCREEN;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_ABOVE){
            wm_states |= WMState::ABOVE;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_BELOW){
            wm_states |= WMState::BELOW;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_DEMANDS_ATTENTION){
            wm_states |= WMState::DEMANDS_ATTENTION;
            continue;
        }
        if (atoms[i] == dsp._NET_WM_STATE_FOCUSED){
            wm_states |= WMState::FOCUSED;
            continue;
        }

        atom_name = XGetAtomName(dsp, atoms[i]);
        TINY_LOG("Unsupported _NET_WM_STATE: %s", atom_name);
        XFree(atom_name);
    }
    XFree(data);
    TINY_LOG("wm_states = %d", wm_states);
}

void Window::on_window_drag_begin(tiny::Object *o, const XEvent &e, void *data)
{
    set_focus();     // check if is not have focus yet
    if (get_maximized()){
        restore(e.xmotion.x, e.xmotion.y);
    }

    start_event = e;
    XGetWindowAttributes(dsp, child, &start_attrs);
    TINY_LOG("start moved");
}


void Window::on_window_drag_motion(tiny::Object *o, const XEvent &e, void *data)
{
    int xdiff = e.xbutton.x_root - start_event.xbutton.x_root;
    int ydiff = e.xbutton.y_root - start_event.xbutton.y_root;

    XMoveWindow(dsp, child, start_attrs.x + xdiff, start_attrs.y + ydiff);
    TINY_LOG("motion");
}


void Window::on_button_press(const XEvent &e, void* data)
{
    set_focus();        // set_focus is not important :-)

    if (e.xbutton.state & Mod1Mask){
        // unblock the pointer, but not send to child
        XAllowEvents(dsp, AsyncPointer, e.xbutton.time);
        XGrabPointer(
                dsp, child, false,
                ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
                GrabModeAsync, GrabModeAsync,
                None, XCreateFontCursor(dsp, XC_hand1), CurrentTime);
        on_drag_begin((Window*)this, e);
        return;
    }

    // resend event to child
    XAllowEvents(dsp, ReplayPointer, e.xbutton.time);
}

void Window::on_button_release(const XEvent &e, void * data){
    // Stop motion
    XUngrabPointer(dsp, CurrentTime);
}

void Window::on_client_message(const XEvent& e, void* data)
{
    if (e.xclient.message_type == dsp._NET_WM_STATE){
        // client want be maximized
        if (e.xclient.data.l[0] == dsp._NET_WM_STATE_ADD)
        {
            if ((Atom)e.xclient.data.l[1] == dsp._NET_WM_STATE_MAXIMIZED_VERT &&
                (Atom)e.xclient.data.l[2] == dsp._NET_WM_STATE_MAXIMIZED_HORZ)
            {
                maximize();
                return;
            }

            char* atom_name = XGetAtomName(dsp, e.xclient.data.l[1]);
            TINY_LOG("Unhandled WM_STATE_ADD request %s", atom_name);
            XFree(atom_name);
        }

        if (e.xclient.data.l[0] == dsp._NET_WM_STATE_REMOVE)
        {
            if ((Atom)e.xclient.data.l[1] == dsp._NET_WM_STATE_MAXIMIZED_VERT &&
                (Atom)e.xclient.data.l[2] == dsp._NET_WM_STATE_MAXIMIZED_HORZ)
            {
                restore();
                return;
            }

            char* atom_name = XGetAtomName(dsp, e.xclient.data.l[1]);
            TINY_LOG("Unhandled WM_STATE_REMOVE request %s", atom_name);
            XFree(atom_name);
        }

        if (e.xclient.data.l[0] == dsp._NET_WM_STATE_TOGGLE)
        {
            char* atom_name = XGetAtomName(dsp, e.xclient.data.l[1]);
            TINY_LOG("Unhandled WM_STATE_TOGLE request %s", atom_name);
            XFree(atom_name);
        }
        return;
    }

    // if (e.xclient.message_type == dsp.WM_CHANGE_STATE){}

    TINY_LOG("try get message_type");
    char* atom_name = XGetAtomName(dsp, e.xclient.message_type);
    TINY_LOG("Unhandled client message type %s", atom_name);
    XFree(atom_name);
}

void Window::on_motion_notify(const XEvent &e, void * data){
    TINY_LOG("MotionNotify...");
    on_drag_motion((Window*)this, e);
}

void Window::on_property_notify(const XEvent &e, void *data)
{

    char *atom_name = XGetAtomName(dsp, e.xproperty.atom);
    TINY_LOG("on_property_notify %s", atom_name);
    XFree(atom_name);

    if (e.xproperty.atom == dsp.WM_PROTOCOLS){
        update_protocols();
        return;
    }
    if (e.xproperty.atom == dsp._NET_WM_STATE){
        update_wm_states();
        return;
    }

}

void Window::on_key_release(const XEvent &e, void* data)
{
    if (e.xkey.state & Mod1Mask
        && e.xkey.keycode == XKeysymToKeycode(dsp, XK_F4))
    {
        close();
    }
}


void Window::set_events()
{
    XSelectInput(dsp, child, PropertyChangeMask);

    XGrabButton(dsp, Button1, AnyModifier, child, true,
            ButtonPressMask|ButtonReleaseMask,
            GrabModeSync, GrabModeSync,
            None, None);
    tiny::x_grab_key(dsp, XKeysymToKeycode(dsp, XK_F4),
            Mod1Mask, child);

    connect_window(ClientMessage, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_client_message));
    connect_window(PropertyNotify, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_property_notify));
    connect_window(KeyRelease,child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_key_release));
    connect_window(ButtonPress, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_button_press));
    connect_window(ButtonRelease, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_button_release));
    connect_window(MotionNotify, child,
            reinterpret_cast<tiny::event_signal_t>(&Window::on_motion_notify));

    on_drag_begin.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Window::on_window_drag_begin));
    on_drag_motion.connect(
            reinterpret_cast<tiny::Object*>(this),
            reinterpret_cast<tiny::object_signal_t>(&Window::on_window_drag_motion));
}

int Window::get_properties(::Window window, std::set<Atom>& properties)
{
    int count;
    properties.clear();

    tiny::Display& display = tiny::get_display();
    Atom * props = XListProperties(display, window, &count);

    TINY_LOG("Window %lx properties:", window);
    char* atom_name;
    for (int i=0; i < count; ++i){
        properties.insert(props[i]);
        atom_name = XGetAtomName(display, props[i]);
        printf("\t%s\n", atom_name);
        XFree(atom_name);
    }
    XFree(props);
    return count;
}

Window::WMType Window::get_net_wm_type(::Window window)
{
    WMType wm_type = WMType::NORMAL;  // Default value
    tiny::Display& display = tiny::get_display();

    Atom return_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char *data = NULL;
    if (XGetWindowProperty(display, window,
                display._NET_WM_WINDOW_TYPE, 0L, 1L,
                false, XA_ATOM,
                &return_type, &size,
                &nitems, &bytes_left, &data) != Success)
    {
        // No _NET_WM_WINDOW_TYPE return
        return wm_type;
    }

    if (bytes_left != 0) {
        XFree(data);
        unsigned long remain = ((size / 8) * nitems) + bytes_left;
        if (XGetWindowProperty(display, window,
                    display._NET_WM_WINDOW_TYPE,
                    0L, remain, false, XA_ATOM,
                    &return_type, &size,
                    &nitems, &bytes_left, &data) != Success)
        {
            // No _NET_WM_WINDOW_TYPE Atoms return
            return wm_type;
        }
    }

    Atom* window_types = reinterpret_cast<Atom*>(data);
    for (unsigned long i = 0; i < nitems; i++){
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_DESKTOP){
            wm_type = WMType::DESKTOP;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_DOCK){
            wm_type = WMType::DOCK;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_TOOLBAR){
            wm_type = WMType::TOOLBAR;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_MENU){
            wm_type = WMType::MENU;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_UTILITY){
            wm_type = WMType::UTILITY;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_SPLASH){
            wm_type = WMType::SPLASH;
            break;
        }
        if (window_types[i] == display._NET_WM_WINDOW_TYPE_DIALOG){
            wm_type = WMType::DIALOG;
            break;
        }
        // NORMAL is default
    }
    XFree(data);
    return wm_type;
}

bool Window::get_motif_hints(::Window window,
        unsigned long& functions, unsigned long& decorations)
{
    functions =
        MotifWMHints::FUNC_ALL |
        MotifWMHints::FUNC_RESIZE |
        MotifWMHints::FUNC_MOVE |
        MotifWMHints::FUNC_MINIMIZE |
        MotifWMHints::FUNC_MAXIMIZE |
        MotifWMHints::FUNC_CLOSE;

    decorations =
        MotifWMHints::DECOR_ALL |
        MotifWMHints::DECOR_BORDER |
        MotifWMHints::DECOR_RESIZE |
        MotifWMHints::DECOR_TITLE |
        MotifWMHints::DECOR_MENU |
        MotifWMHints::DECOR_MINIMIZE |
        MotifWMHints::DECOR_MAXIMIZE;

    tiny::Display& display = tiny::get_display();
    Atom returned_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    MotifWMHints *hints = nullptr;

    if (XGetWindowProperty(display, window,
                display._MOTIF_WM_HINTS,
                0L, MotifWMHints::property_length,
                false, display._MOTIF_WM_HINTS,
                &returned_type, &size,
                &nitems, &bytes_left,
                reinterpret_cast<uint8_t**>(&hints)) != Success || !nitems)
    {
        return false;
    }
    if (hints->flags & MotifWMHints::FLAG_FUNCTIONS){
        functions = hints->functions;
    }
    if (hints->flags & MotifWMHints::FLAG_DECORATIONS){
        decorations = hints->decorations;
    }

    TINY_LOG("_MOTIF: flags: %lu func: %lu dec: %lu in:%ld status:%lu",
            hints->flags,
            hints->functions,
            hints->decorations,
            hints->input_mode,
            hints->status);

    XFree(hints);

    return true;
}

} // namespace wm
