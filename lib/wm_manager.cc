#include <vector>

#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#include "wm_manager.h"
#include "wm_wrapped.h"
#include "x_util.h"

namespace tiny {
    extern Handlers handlers;
    extern std::shared_ptr<Display> display;
}

namespace wm {

Manager::Manager(::Display* display, ::Window root):
    tiny::Object(),
    display(display),
    root(root),
    wm_panel(), wm_run(), running(true)
{
    //set_supported();
    XSetWindowBackground(display, root, tiny::theme.root_background);
    XSync(display, True);

    XDefineCursor(display, root, XCreateFontCursor(display, XC_arrow));

    ::Window rv_root;
    ::Window rv_parent;
    ::Window *rv_child;
    uint32_t rv_ccount;

    /*
    XChangeProperty(display, root, display._NET_SUPPORTED, XA_ATOM,
                    32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(display.EWMH), 1);
    */

    XGrabServer(display);       // lock the X server for new events
    XQueryTree(display, root, &rv_root, &rv_parent, &rv_child, &rv_ccount);
    if (rv_ccount){
        rv_ccount -= 1;
        XWindowAttributes attrs;

        for (uint32_t i = 0; i <= rv_ccount; ++i){
            XGetWindowAttributes(display, rv_child[i], &attrs);
            if (attrs.map_state == IsUnmapped){
                continue;   // will be mapped later
            }
            if (attrs.override_redirect){
                continue;   // menu, special tools (idesk) and so on
            }

            std::set<Atom> properties;
            Window::get_properties(rv_child[i], properties);

            if (properties.count(tiny::display->_NET_WM_WINDOW_TYPE))
            {
                Window::WMType wm_type = Window::get_net_wm_type(rv_child[i]);
                switch (wm_type) {
                    case Window::WMType::DESKTOP:
                    case Window::WMType::DOCK:
                    case Window::WMType::SPLASH:
                        continue; // do not manage this window type
                    default:
                        break;
                }
            }

            Window* window = nullptr;
            unsigned long functions;
            unsigned long decorations;
            if (Window::get_motif_hints(rv_child[i], functions, decorations)
                    && !decorations)
            {
                window = Window::create(root, rv_child[i], attrs, functions);
            } else {
                window = Wrapped::create(root, rv_child[i], attrs,
                                         functions, decorations);
            }

            wm_windows[rv_child[i]] = window;
            wm_tops.push_back(window);
            window->on_focus.connect(this,
                    static_cast<tiny::object_signal_t>(&Manager::on_window_focus));
            if (i == rv_ccount){
                window->set_focus();
            }
        }
        XFree(rv_child);
    }

    wm_panel.realize(root, 0, 0);
    wm_panel.set_events();
    wm_panel.map_all();

    wm_run.realize(root, 0, 0);
    wm_run.set_events();

    wm_panel.menu_user.logout.on_click.connect(
            this,
            static_cast<tiny::object_signal_t>(&Manager::on_logout));

    XClearWindow(display, root);
    set_events();
    XUngrabServer(display);     // unlock the X server
}

Manager::~Manager(){
    // Alt + Tab
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_Tab),
              Mod1Mask, root);
    // Shift + Alt + Tab
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_Tab),
              ShiftMask|Mod1Mask, root);
    // Alt + F2
    tiny::x_ungrab_key(display, XKeysymToKeycode(display, XK_F2),
              Mod1Mask, root);

    XCloseDisplay(display);
}

void Manager::set_events()
{
    XSelectInput(display, root,
            SubstructureNotifyMask|SubstructureRedirectMask);

    // Alt + Tab
    tiny::x_grab_key(display, XKeysymToKeycode(display, XK_Tab),
              Mod1Mask, root);
    // Shift + Alt + Tab
    tiny::x_grab_key(display, XKeysymToKeycode(display, XK_Tab),
              ShiftMask|Mod1Mask, root);
    // Alt + F2
    tiny::x_grab_key(display, XKeysymToKeycode(display, XK_F2),
              Mod1Mask, root);
}

void Manager::set_supported()
{
    Atom supported[] = {
        tiny::display->_NET_SUPPORTED,
        tiny::display->_NET_WM_NAME,
        tiny::display->_NET_WM_VISIBLE_NAME,
        tiny::display->_NET_WM_WINDOW_TYPE
    };

    XChangeProperty(display, root, tiny::display->_NET_SUPPORTED, XA_ATOM,
            32, PropModeReplace,
            reinterpret_cast<unsigned char *>(supported),
            sizeof(supported)/sizeof(Atom));
}

void Manager::main_loop()
{
    XEvent event;

    while (running){
        XNextEvent(display, &event);
        /*TINY_LOG("Event: %s (%lx)\n",
                 event_to_string(event), event.xany.window);*/

        auto signal_id = std::make_pair(event.type, event.xany.window);

        // call manager handlers first
        if (tiny::handlers.count(signal_id)){
            tiny::handlers.call_hanlder(signal_id, event);
            continue;
        }

        switch(event.type) {
            case KeyPress:      // shortcats
                on_key_press(event.xkey);
                break;
            case KeyRelease:    // shortcats
                on_key_release(event.xkey);
                break;
            case UnmapNotify:   // need for delete windows frames (wm_windows)
                on_unmap_notify(event.xunmap);
                break;
            case MapRequest:    // need to create windows frames for new windows
                on_map_request(event.xmaprequest);
                break;
            case ConfigureRequest:  // need for resize requests from app windows
                on_configure_request(event.xconfigurerequest);
                break;
            case ClientMessage:
                on_client_message(event.xclient);
                print_wm_state(event.xclient.window);
                break;
            case CreateNotify:
                TINY_LOG("CreateNotify for %lx -> %d;%d [%dx%d]",
                        event.xany.window,
                        event.xcreatewindow.x,
                        event.xcreatewindow.y,
                        event.xcreatewindow.width,
                        event.xcreatewindow.height);
                break;
            default:
                // TODO: co dělají ConfigureNotify? Chodí před _NET_WM_USER_TIME
                // Na Gedit chodí ještě další další notifikace... proč?
#ifdef DEBUG
                fprintf(stderr, "Unhandled event: %s (%lx)\n",
                        event_to_string(event), event.xany.window);
#endif
                break;
        }
    }
}

void Manager::activate_next_window()
{
    if (wm_windows.size() == 0){
        return;     // no window to activate
    }

    Window * next = *wm_tops.cbegin();
    for (auto it = wm_tops.cbegin(); it != wm_tops.cend(); ++it){
        if (active == *it) {
            if (++it == wm_tops.cend()) {
                it = wm_tops.cbegin();
            }
            next = *it;
            break;
        }
    }

    if (next->get_minimized()){
        next->restore();
    }
    next->set_focus();
}

void Manager::activate_prev_window()
{
    if (wm_tops.size() == 0){
        return;     // no window to activate
    }

    Window* prev = *wm_tops.crbegin();
    for (auto it = wm_tops.crbegin(); it != wm_tops.crend(); ++it) {
        if (active == *it) {
            if (++it == wm_tops.crend()) {
                it = wm_tops.crbegin();
            }
            prev = *it;
            break;
        }
    }
    if (prev->get_minimized()){
        prev->restore();
    }
    prev->set_focus();
}

void Manager::on_window_focus(tiny::Object *o, const XEvent &e, void *data){
    active = reinterpret_cast<Window*>(o);
}

void Manager::on_logout(tiny::Object *o, const XEvent &e, void *data){
    running = false;
}


void Manager::on_key_press(const XKeyEvent &xkey)
{
    /* Alt F2 */
    if (xkey.keycode == XKeysymToKeycode(display, XK_F2))
    {
        wm_run.popup();
        key_done = true;
        return;
    }
    key_done = false;
}

void Manager::on_key_release(const XKeyEvent &xkey)
{
    if (key_done){
        return; // key is handled in on_key_press
    }

    /* Alt + Tab or Alt + Shift + Tab */
    if (xkey.keycode == XKeysymToKeycode(display, XK_Tab)
            && xkey.state & Mod1Mask)
    {
        if (xkey.state & ShiftMask){
            activate_prev_window();
        } else {
            activate_next_window();
        }
        return;
    }

    fprintf(stderr, "%lu Manager::on_key_release Unhalded key event: %x\n",
            xkey.time, xkey.keycode);
}

void Manager::on_unmap_notify(const XUnmapEvent &e)
{
    // TODO: could be append to signal handlers on wm_window.window
    if (wm_windows.count(e.window)){
        Window * window = wm_windows[e.window];
        wm_tops.remove(window);
        delete (window);
        wm_windows.erase(e.window);
        activate_prev_window();
    }
}

void Manager::on_map_request(const XMapRequestEvent &e)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, e.window, &attrs);
    if (attrs.map_state == IsViewable){
        return;
    }
    if (attrs.override_redirect){
        return;     // menu, special tools (idesk) and so on
    }

    print_wm_state(e.window);

    std::set<Atom> properties;
    Window::get_properties(e.window, properties);

    if (properties.count(tiny::display->_NET_WM_WINDOW_TYPE))
    {
        Window::WMType wm_type = Window::get_net_wm_type(e.window);
        switch (wm_type) {
            case Window::WMType::DESKTOP:
            case Window::WMType::DOCK:
            case Window::WMType::SPLASH:
                XMapWindow(display, e.window);
                return; // do not manage this window type
            default:
                break;
        }
    }

    Window* window = nullptr;
    unsigned long functions;
    unsigned long decorations;

    if (Window::get_motif_hints(e.window, functions, decorations)
            && !decorations)
    {
        window = Window::create(root, e.window, attrs, functions);
    } else {
        window = Wrapped::create(root, e.window, attrs, functions, decorations);
    }

    wm_windows[e.window] = window;
    wm_tops.push_back(window);
    XMapWindow(display, e.window);
    window->on_focus.connect(this,
            static_cast<tiny::object_signal_t>(&Manager::on_window_focus));
    window->set_focus();
}

void Manager::on_configure_request(const XConfigureRequestEvent &e)
{
    // could be connect as signal for wm_window.chilid
    XWindowChanges ch;
    ch.x = e.x;
    ch.y = e.y;
    ch.width = e.width;
    ch.height = e.height;
    ch.border_width = e.border_width;
    ch.sibling = e.above;
    ch.stack_mode = e.detail;

    TINY_LOG("win size: %d x %d -> [%d x %d]", e.x, e.y, e.width, e.height);
    XConfigureWindow(display, e.window, e.value_mask, &ch);
    // printf(" > Resize to %dx%d\n", e.width, e.height);

    // XXX: Why this ?
    if (false && wm_windows.count(e.window)){
        const ::Window frame = wm_windows[e.window]->get_child();
        ch.width -= 2;
        ch.height -= 22;
        XConfigureWindow(display, frame, e.value_mask, &ch);
        printf(" > Resize frame to %dx%d\n", e.width, e.height);
    }
}

void Manager::on_client_message(const XClientMessageEvent& xclient)
{
    char * prop_name = nullptr;
    prop_name = XGetAtomName(display, xclient.message_type);
    TINY_LOG("Recieve ClientMessage %s (%ld, %ld, %ld, %ld)",
            prop_name,
            xclient.data.l[0],  // action
            xclient.data.l[1],  // first property
            xclient.data.l[2],  // second property
            xclient.data.l[3]); // source indication
    XFree(prop_name);

    if (xclient.message_type == tiny::display->_NET_WM_STATE){
        for (uint8_t i = 1; i <= 2; i++){
            prop_name = XGetAtomName(display,
                    (Atom)xclient.data.l[1]);
            TINY_LOG("\t _NET_WM_STATE: %s", prop_name);
            XFree(prop_name);
        }
    }

    if (xclient.data.l[0] == tiny::Display::_NET_WM_STATE_ADD){
        if ((Atom)xclient.data.l[1] == tiny::display->_NET_WM_STATE_MAXIMIZED_VERT &&
                (Atom)xclient.data.l[2] == tiny::display->_NET_WM_STATE_MAXIMIZED_HORZ)
        {
            XWindowAttributes root_attrs;   //TODO: get_screen size
            XGetWindowAttributes(display, root, &root_attrs);
            XMoveResizeWindow(display, xclient.window, 0, 0,
                    root_attrs.width, root_attrs.height);

            std::vector<Atom> atoms;
            atoms.push_back(tiny::display->_NET_WM_STATE_MAXIMIZED_VERT);
            atoms.push_back(tiny::display->_NET_WM_STATE_MAXIMIZED_HORZ);

            XChangeProperty(display, xclient.window,
                    tiny::display->_NET_WM_STATE, XA_ATOM,
                    32, PropModeReplace,
                    reinterpret_cast<unsigned char *>(&(atoms[0])),
                    atoms.size());
        }
    }
}

void Manager::print_wm_state(::Window window)
{
    Atom returned_type;
    int size;
    unsigned long nitems;
    unsigned long bytes_left;
    unsigned char *data = NULL;
    TINY_LOG("Try to get _NET_WM_STATE...");

    if (XGetWindowProperty(*tiny::display, window,
                tiny::display->_NET_WM_STATE, 0L, 1L,
                false, XA_ATOM,
                &returned_type, &size,
                &nitems, &bytes_left, &data) != Success || !nitems)
    {
        TINY_LOG("No _NET_WM_STATE...");
        return;
    }
    TINY_LOG("\tbytes left: %ld nitems: %ld", bytes_left, nitems);

    if (bytes_left != 0) {  // Fetch all states...
        XFree(data);
        unsigned long remain = ((size / 8) * nitems) + bytes_left;
        if (XGetWindowProperty(*tiny::display, window,
                    tiny::display->_NET_WM_STATE,  0L, remain,
                    false, XA_ATOM,
                    &returned_type, &size,
                    &nitems, &bytes_left, &data) != Success)
        {
            TINY_LOG("No other _NET_WM_STATE...");
            return;
        }
    }

    char* atom_name;
    Atom* atoms = reinterpret_cast<Atom*>(data);
    for (unsigned long i = 0; i < nitems; i++){
        atom_name = XGetAtomName(display, atoms[i]);
        TINY_LOG("\t _NET_WM_STATE: %s", atom_name);
        XFree(atom_name);
    }
    XFree(data);
}

} // namespace wm
