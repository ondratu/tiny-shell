#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "window_manager.h"
#include "wm_theme.h"
#include "x_util.h"

extern WMHandlers wm_handlers;

WindowManager::~WindowManager()
{
    XCloseDisplay(display);
}

WindowManager::WindowManager(Display* display):
        display(display), root(XDefaultRootWindow(display))
{
    XDefineCursor(display, root, XCreateFontCursor(display, XC_arrow));

    XSetWindowBackground(display, root, WM_ROOT_BACKGROUND);
    XClearWindow(display, root);
}

std::unique_ptr<WindowManager> WindowManager::create()
{
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        fprintf(stderr, "Failed to open X display");
        return nullptr;
    }

    auto wm = std::unique_ptr<WindowManager>(new WindowManager(display));
    wm->set_events();
    return wm;
}

void WindowManager::set_events()
{
    XSelectInput(display, root,
        SubstructureNotifyMask|SubstructureRedirectMask);
    XSync(display, false);

    XGrabKey(           // Alt + Tab
        display, XKeysymToKeycode(display, XK_Tab),
        Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(           // Shift + Alt + Tab
        display, XKeysymToKeycode(display, XK_Tab),
        Mod1Mask|ShiftMask, root, true, GrabModeAsync, GrabModeAsync);


    XGrabKey(           // Tab              // XXX Debug only
        display, XKeysymToKeycode(display, XK_F2),
        AnyModifier, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(           // Shift + Tab      // XXX Debug only
        display, XKeysymToKeycode(display, XK_F2),
        ShiftMask, root, true, GrabModeAsync, GrabModeAsync);


    XGrabKey(           // Alt + F4
        display, XKeysymToKeycode(display, XK_F4),
        Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(           // Shift + Tab
        display, XKeysymToKeycode(display, XK_F4),
        ShiftMask, root, true, GrabModeAsync, GrabModeAsync);

    // ShiftMask not work without AnyModifier
    XGrabKey(           // Shift + Tab      // XXX Debug only
        display, XKeysymToKeycode(display, XK_F4),
        AnyModifier, root, true, GrabModeAsync, GrabModeAsync);
}

void WindowManager::main_loop()
{
    XWindowAttributes attr;
    XEvent event;

    while (true){
        XNextEvent(display, &event);

        auto signal_id = std::make_pair(event.type, event.xany.window);

        // call manager handlers first
        if (wm_handlers.count(signal_id)){
            wm_handlers.call_hanlder(signal_id, event);
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

void WindowManager::activate_next_window()
{
    if (wm_windows.size() == 0){
        return;     // no window to activate
    }

    Window active;
    int revert;
    XGetInputFocus(display, &active, &revert);

    WMWindow * next = *wm_tops.cbegin();
    for (auto it = wm_tops.cbegin(); it != wm_tops.cend(); ++it){
        if (active == (*it)->get_window()){
            if (++it == wm_tops.cend()){
                it = wm_tops.cbegin();
            }
            next = *it;
            break;
        }
    }

    printf("next is minimized: %d\n", next->get_minimized());
    if (next->get_minimized()){
        next->restore();
    }

    // FIXME: Not work / pointer is out, applications not accept kayboard

    // XRaiseWindow(display, next->window);
    // XSetInputFocus(display, next->window, RevertToPointerRoot, CurrentTime);
    next->set_focus();
}

void WindowManager::activate_prev_window()
{
    if (wm_tops.size() == 0){
        return;     // no window to activate
    }

    Window active;
    int revert;
    XGetInputFocus(display, &active, &revert);

    WMWindow* prev = *wm_tops.crbegin();
    for (auto it = wm_tops.crbegin(); it != wm_tops.crend(); ++it){
        if (active == (*it)->get_window()){
            if (++it == wm_tops.crend()){
                it = wm_tops.crbegin();
            }
            prev = *it;
            break;
        }
    }
    if (prev->get_minimized()){
        prev->restore();
    }
    //XRaiseWindow(display, prev->window);
    //XSetInputFocus(display, prev->window, RevertToPointerRoot, CurrentTime);
    prev->set_focus();
}

void WindowManager::on_key_press(const XKeyEvent &e)
{
    /* Alt F4 */
    if (e.keycode == XKeysymToKeycode(display, XK_F4) &&
       ((e.state & ShiftMask) == ShiftMask ||      // XXX: debug only
        (e.state & Mod1Mask) == Mod1Mask))
    {
        Window active;
        int revert;
        XGetInputFocus(display, &active, &revert);
        if (active > 1){    // not to close root window
            // TODO: do_close_window(active);
        }
    }

    fprintf(stderr, "Unhalded key event: %u\n", e.keycode);
}

void WindowManager::on_key_release(const XKeyEvent &e)
{
    /* Alt + Tab or Alt + Shift + Tab */
    if (e.keycode == XKeysymToKeycode(display, XK_Tab) ||
        e.keycode == XKeysymToKeycode(display, XK_F2))
    {
        if (e.type == KeyRelease){
            if ((e.state & ShiftMask) == ShiftMask){
                printf("call activate_prev_window\n");
                activate_prev_window();
            } else {
                printf("call activate_next_window\n");
                activate_next_window();
            }
        }
        return;
    }

    fprintf(stderr, "Unhalded key event: %u\n", e.keycode);
}

void WindowManager::on_unmap_notify(const XUnmapEvent &e)
{
    // TODO: could be append to signal handlers on wm_window.window
    if (wm_windows.count(e.window)){
        WMWindow * window = wm_windows[e.window];
        wm_tops.remove(window);
        delete (window);
        wm_windows.erase(e.window);
    }
}

void WindowManager::on_map_request(const XMapRequestEvent &e)
{
    WMWindow * window = WMWindow::create(display, root, e.window);
    wm_windows[e.window] = window;
    wm_tops.push_back(window);
    XMapWindow(display, e.window);
}

void WindowManager::on_configure_request(const XConfigureRequestEvent &e)
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
        const Window frame = wm_windows[e.window]->get_window();
        ch.width -= 2;
        ch.height -= 22;
        XConfigureWindow(display, frame, e.value_mask, &ch);
        printf(" > Resize frame to %dx%d\n", e.width, e.height);
    }
}
