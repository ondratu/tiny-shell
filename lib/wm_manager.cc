#include <X11/cursorfont.h>

#include "wm_manager.h"
#include "x_util.h"

namespace tiny {
    extern Handlers handlers;
}

namespace wm {

Manager::Manager():
    tiny::Object(),
    display(tiny::get_display())
{
    root = XDefaultRootWindow(display);

    XDefineCursor(display, root, XCreateFontCursor(display, XC_arrow));

    ::Window rv_root;
    ::Window rv_parent;
    ::Window *rv_child;
    uint32_t rv_ccount;

    XGrabServer(display);       // lock the X server for new events
    XQueryTree(display, root, &rv_root, &rv_parent, &rv_child, &rv_ccount);
    if (rv_ccount){
        rv_ccount -= 1;
        for (uint32_t i = 0; i <= rv_ccount; ++i){
            Window * window = Window::create(root, rv_child[i]);
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
    XSetWindowBackground(display, root, WM_ROOT_BACKGROUND);
    XClearWindow(display, root);
    set_events();
    XUngrabServer(display);     // unlock the X server
}

Manager::~Manager(){
    XCloseDisplay(display);
}

void Manager::set_events()
{
    XSelectInput(display, root,
            SubstructureNotifyMask|SubstructureRedirectMask);

    XGrabKey(           // Alt + Tab
            display, XKeysymToKeycode(display, XK_Tab),
            Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(           // Shift + Alt + Tab
            display, XKeysymToKeycode(display, XK_Tab),
            Mod1Mask|ShiftMask, root, true, GrabModeAsync, GrabModeAsync);

    XGrabKey(           // Alt + F4
            display, XKeysymToKeycode(display, XK_F4),
            Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(           // Alt + F2
            display, XKeysymToKeycode(display, XK_F2),
            Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
}

void Manager::main_loop()
{
    XWindowAttributes attr;
    XEvent event;

    while (true){
        XNextEvent(display, &event);

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
            default:
#ifdef DEBUG
                fprintf(stderr, "Unhandled event: %s (%x)\n",
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
    active = static_cast<Window*>(o);
}

void Manager::on_key_press(const XKeyEvent &e)
{
    /* Alt F4 */
    if (e.keycode == XKeysymToKeycode(display, XK_F4))
    {
        ::Window active;
        int revert;
        XGetInputFocus(display, &active, &revert);
        if (active > 1){    // not to close root window
            // TODO: do_close_window(active);
            printf("close the window...\n");
        }
        key_done = true;
        return;
    }

    /* Alt F2 */
    if (e.keycode == XKeysymToKeycode(display, XK_F2))
    {
        printf("TODO: run dialog\n");
        key_done = true;
        return;
    }
    fprintf(stderr, "%lu Manager::on_key_press Unhalded key event: %x\n",
            e.time, e.keycode);
    key_done = false;
}

void Manager::on_key_release(const XKeyEvent &e)
{
    printf("%lu Manager::on_key_release %x\n", e.time, e.keycode);
    if (key_done){
        return;         // key is handled in on_key_press
    }

    /* Alt + Tab or Alt + Shift + Tab */
    if (e.keycode == XKeysymToKeycode(display, XK_Tab))
    {
        if (e.state & ShiftMask){
            printf("call activate_prev_window\n");
            activate_prev_window();
        } else {
            printf("call activate_next_window\n");
            activate_next_window();
        }
        return;
    }

    fprintf(stderr, "%lu Manager::on_key_release Unhalded key event: %x\n",
            e.time, e.keycode);
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
    if (attrs.override_redirect){
        TINY_LOG("window %lx hase override_redirect", e.window);
        return;     // do not manage this window
    }

    Window * window = Window::create(root, e.window);
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

    XConfigureWindow(display, e.window, e.value_mask, &ch);
    printf(" > Resize to %dx%d\n", e.width, e.height);

    // XXX: Why this ?
    if (wm_windows.count(e.window)){
        const ::Window frame = wm_windows[e.window]->get_window();
        ch.width -= 2;
        ch.height -= 22;
        XConfigureWindow(display, frame, e.value_mask, &ch);
        printf(" > Resize frame to %dx%d\n", e.width, e.height);
    }
}

} // namespace wm
