#include <vector>

#include <X11/cursorfont.h>
#include <X11/Xatom.h>

#include "object.h"
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
    wm_panel(), wm_run(), wm_dock(), running(true)
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
            if (!window->skip_taskbar()){
                wm_dock.add(window);
            }
            window->on_focus.connect(this,
                    static_cast<tiny::object_signal_t>(&Manager::on_window_focus));
            if (i == rv_ccount){
                window->set_focus();
            }
        }
        XFree(rv_child);
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    wm_panel.realize(root, 0, 0);
    wm_panel.set_events();
    wm_panel.map_all();

    wm_dock.realize(root,
            attrs.width/2,
            attrs.height - wm_dock.get_height()-tiny::theme.wm_dock_border);
    wm_dock.set_events();
    wm_dock.map_all();

    wm_run.realize(root, 0, 0);
    wm_run.set_events();

    wm_panel.menu_user.logout.on_click.connect(
            this,
            static_cast<tiny::object_signal_t>(&Manager::on_logout));

    XClearWindow(display, root);
    set_events();
    XSetErrorHandler(&Manager::on_error);
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

    if (old_error_handler){
        XSetErrorHandler(old_error_handler);
    }
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
        ::Window window = tiny::window_of_event(event);

        /*TINY_LOG("Event: %s (%lx)\n\t",
                 event_to_string(event), window);*/

        auto signal_id = std::make_pair(event.type, window);

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
            /*
            case CreateNotify:
                if (!event.xcreatewindow.override_redirect){
                    TINY_LOG("CreateNotify for %lx -> %d;%d [%dx%d] !!",
                            event.xcreatewindow.window,
                            event.xcreatewindow.x,
                            event.xcreatewindow.y,
                            event.xcreatewindow.width,
                            event.xcreatewindow.height);
                }
                break;
            */
            default:
#ifdef DEBUG
                fprintf(stderr, "Unhandled event: %s (%lx)\n",
                        event_to_string(event), window);
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

    active = nullptr;
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
    active = nullptr;
    if (prev->get_minimized()){
        prev->restore();
    }
    prev->set_focus();
}

void Manager::on_window_focus(tiny::Object *o, const XEvent &e, void *data)
{
    if (active){
        active->return_focus();
        wm_dock.redraw(active, e);
    }
    active = reinterpret_cast<Window*>(o);
    wm_dock.redraw(active, e);
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
        if (window->get_minimized()){
            return;
        }
        if (active == window){
            activate_prev_window();
        }
        wm_tops.remove(window);
        wm_dock.remove(window);
        delete (window);
        wm_windows.erase(e.window);
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
    if (!window->skip_taskbar()){
        wm_dock.add(window);
    }
    XMapWindow(display, e.window);
    window->on_focus.connect(this,
            static_cast<tiny::object_signal_t>(&Manager::on_window_focus));
    window->set_focus();
}

void Manager::on_configure_request(const XConfigureRequestEvent &e)
{
    XWindowChanges ch;
    ch.x = e.x;
    ch.y = e.y;
    ch.width = e.width;
    ch.height = e.height;
    ch.border_width = e.border_width;
    ch.sibling = e.above;
    ch.stack_mode = e.detail;

    XConfigureWindow(display, e.window, e.value_mask, &ch);
}

int Manager::on_error(::Display* display, XErrorEvent* error)
{
    char buffer[255];
    XGetErrorText(display, error->error_code, buffer, 255);
    tiny::error("XError %s (%d) opcode %d/%d on res %lx", buffer,
            error->error_code, error->request_code, error->minor_code,
            error->resourceid);
    return 0;
}


} // namespace wm
