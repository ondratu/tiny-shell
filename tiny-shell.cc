#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include <stdio.h>

#include <list>

#include "wm_window.h"

#define ROOT_BACGROUND 0x729FCF

extern Handlers_t handlers;

const char * event_to_string(const XEvent e){
    switch (e.type){
        case ButtonPress:
            return "ButtonPress";
        case ButtonRelease:
            return "ButtonRelease";
        case CirculateNotify:
            return "CirculateNotify";
        case CirculateRequest:
            return "CirculateRequest";
        case ClientMessage:
            return "ClientMessage";
        case ColormapNotify:
            return "ColormapNotify";
        case ConfigureNotify:
            return "ConfigureNotify";
        case ConfigureRequest:
            return "ConfigureRequest";
        case CreateNotify:
            return "CreateNotify";
        case DestroyNotify:
            return "DestroyNotify";
        case EnterNotify:
            return "EnterNotify";
        case Expose:
            return "Expose";
        case FocusIn:
            return "FocusIn";
        case FocusOut:
            return "FocusOut";
        case GraphicsExpose:
            return "GraphicsExpose";
        case KeymapNotify:
            return "KeymapNotify";
        case KeyPress:
            return "KeyPress";
        case KeyRelease:
            return "KeyRelease";
        case LeaveNotify:
            return "LeaveNotify";
        case MapNotify:
            return "MapNotify";
        case MappingNotify:
            return "MappingNotify";
        case MapRequest:
            return "MapRequest";
        case MotionNotify:
            return "MotionNotify";
        case NoExpose:
            return "NoExpose";
        case PropertyNotify:
            return "PropertyNotify";
        case ReparentNotify:
            return "ReparentNotify";
        case ResizeRequest:
            return "ResizeRequest";
        case SelectionClear:
            return "SelectionClear";
        case SelectionNotify:
            return "SelectionNotify";
        case SelectionRequest:
            return "SelectionRequest";
        case UnmapNotify:
            return "UnmapNotify";
        case VisibilityNotify:
            return "VisibilityNotify";
        default:
            return "Unknown type";
    }
}

bool xerror = false;

int on_error(Display * display, XErrorEvent * error){
    fprintf(stderr, "[E]: X11 error\n");
    xerror = true;
    return 1;
}

class WindowManager {
  public:
    // typedef void(WindowManager::*handler_t)(const XEvent&);

    ~WindowManager(){
        XCloseDisplay(display);
    }

    static std::unique_ptr<WindowManager> create(){
        Display* display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            fprintf(stderr, "Failed to open X display");
            return nullptr;
        }

        return std::unique_ptr<WindowManager>(new WindowManager(display));
    }

    void events_setup(){
        XSelectInput(display, root,
                     SubstructureNotifyMask|SubstructureRedirectMask);
        XSync(display, false);

        XGrabKey(           // Alt + Tab
            display, XKeysymToKeycode(display, XK_Tab),
            Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
        XGrabKey(           // Shift + Alt + Tab
            display, XKeysymToKeycode(display, XK_Tab),
            Mod1Mask|ShiftMask, root, true, GrabModeAsync, GrabModeAsync);

        XGrabKey(           // Tab
            display, XKeysymToKeycode(display, XK_F2),
            AnyModifier, root, true, GrabModeAsync, GrabModeAsync);
        XGrabKey(           // Shift + Tab
            display, XKeysymToKeycode(display, XK_F2),
            ShiftMask, root, true, GrabModeAsync, GrabModeAsync);

        XGrabKey(           // Alt + F4
            display, XKeysymToKeycode(display, XK_F4),
            Mod1Mask, root, true, GrabModeAsync, GrabModeAsync);
        XGrabKey(           // Shift + Tab
            display, XKeysymToKeycode(display, XK_F4),
            ShiftMask, root, true, GrabModeAsync, GrabModeAsync);

        // ShiftMask not work without AnyModifier
        XGrabKey(           // Shift + Tab
            display, XKeysymToKeycode(display, XK_F4),
            AnyModifier, root, true, GrabModeAsync, GrabModeAsync);
    }


    void run(){
        XWindowAttributes attr;
        XEvent event;

        while (true){
            XNextEvent(display, &event);

            auto handler_id = std::make_pair(event.type,
                                             event.xany.window);
            // call manager handlers first
            if (handlers.count(handler_id)){
                //&(handlers[handler_id])(event);
                auto val = handlers[handler_id];
                handler_t handler = std::get<0>(val);
                handler(event, std::get<1>(val), std::get<2>(val));
                continue;
            }

            switch(event.type) {
                case KeyPress:
                    printf(" press key: %x (%x) [%x]\n",
                           event.xkey.keycode, event.xkey.state, event.type);
                    on_key_event(event.xkey);
                    break;
                case KeyRelease:
                    printf(" release key: %x (%x) [%x]\n",
                           event.xkey.keycode, event.xkey.state, event.type);
                    on_key_event(event.xkey);
                    break;
                case UnmapNotify:
                    on_unmap_notify(event.xunmap);
                    break;
                case MapNotify:
                    break;
                case MapRequest:
                    on_map_request(event.xmaprequest);
                    break;
                case ConfigureRequest:
                    on_configure_request(event.xconfigurerequest);
                    break;
                case ConfigureNotify: // Ignore
                case Expose:
                    break;
                case LeaveNotify:
                    printf("LeaveNotify ...\n");
                    //XSetInputFocus(display, root, RevertToPointerRoot, CurrentTime);
                    break;
                default:
                    fprintf(stderr, "Unhandled event: %s (%x)\n",
                            event_to_string(event), event.xany.window);
            }
        }
    }

  private:

    WindowManager(Display* display):
        display(display), root(XDefaultRootWindow(display)),
        screen(DefaultScreen(display)), gc(XDefaultGC(display, screen)),
        WM_DELETE_WINDOW(XInternAtom(display, "WM_DELETE_WINDOW", false)),
        WM_PROTOCOLS(XInternAtom(display, "WM_PROTOCOLS", false)),
        moving(0), resizing(0)
    {
        XDefineCursor(display, root,
        XCreateFontCursor(display, XC_arrow));

        XSetWindowBackground(display, root, ROOT_BACGROUND);
        XClearWindow(display, root);
        //XSetErrorHandler(on_error);

        printf("Root depth: %d\n", DefaultDepth(display, screen));
        printf("CurrentTime: %d\n", CurrentTime);

    }

    /* key events */
    void on_key_event(const XKeyEvent &e){
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

        /* Alt F4 */
        if (e.type == KeyPress &&
            e.keycode == XKeysymToKeycode(display, XK_F4) &&
            ((e.state & ShiftMask) == ShiftMask ||
             (e.state & Mod1Mask) == Mod1Mask))
        {
            Window active;
            int revert;
            XGetInputFocus(display, &active, &revert);
            if (active > 1){    // not to close root window
                // TODO: do_close_window(active);
            }
        }

        if (e.keycode == XKeysymToKeycode(display, XK_F4)){
            printf("F4 \n");
        }

        fprintf(stderr, "Unhalded key event: %u\n", e.keycode);
    }

    /* unmap frame */
    void on_unmap_notify(const XUnmapEvent &e){
        if (wm_windows.count(e.window)){
            WMWindow * window = wm_windows[e.window];
            wm_tops.remove(window);
            delete (window);
            wm_windows.erase(e.window);
        }
    }

    /* map window to frame */
    void on_map_request(const XMapRequestEvent &e){
        WMWindow * window = WMWindow::create(display, root, e.window);
        wm_windows[e.window] = window;
        wm_tops.push_back(window);
        XMapWindow(display, e.window);
    }

    void on_configure_request(const XConfigureRequestEvent &e){
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
            const Window frame = wm_windows[e.window]->window;
            ch.width -= 2;
            ch.height -= 22;
            XConfigureWindow(display, frame, e.value_mask, &ch);
            printf(" > Resize frame to %dx%d\n", e.width, e.height);
        }
    }

    /* switch to next window */
    void activate_next_window(){
        if (wm_windows.size() == 0){
            return;     // no window to activate
        }

        Window active;
        int revert;
        XGetInputFocus(display, &active, &revert);

        WMWindow * next = *wm_tops.cbegin();
        for (auto it = wm_tops.cbegin(); it != wm_tops.cend(); ++it){
            if (active == (*it)->window){
                if (++it == wm_tops.cend()){
                    it = wm_tops.cbegin();
                }
                next = *it;
                break;
            }
        }

        printf("next is minimized: %d\n", next->is_minimized);
        if (next->is_minimized){
            printf("minimized ---\n");
            XMapWindow(display, next->window);
            next->is_minimized = false;
        }

        // FIXME: Not work / pointer is out, applications not accept kayboard

        // XRaiseWindow(display, next->window);
        // XSetInputFocus(display, next->window, RevertToPointerRoot, CurrentTime);
        next->set_focus();
    }

    /* switch to preview window */
    void activate_prev_window(){
        if (wm_tops.size() == 0){
            return;     // no window to activate
        }

        Window active;
        int revert;
        XGetInputFocus(display, &active, &revert);

        WMWindow* prev = *wm_tops.crbegin();
        for (auto it = wm_tops.crbegin(); it != wm_tops.crend(); ++it){
            if (active == (*it)->window){
                if (++it == wm_tops.crend()){
                    it = wm_tops.crbegin();
                }
                prev = *it;
                break;
            }
        }
        if (prev->is_minimized){
            XMapWindow(display, prev->window);
            prev->is_minimized = false;
        }
        //XRaiseWindow(display, prev->window);
        //XSetInputFocus(display, prev->window, RevertToPointerRoot, CurrentTime);
        prev->set_focus();
    }

    Display* display;
    const Window root;
    int screen;
    GC gc;

    const Atom WM_DELETE_WINDOW;
    const Atom WM_PROTOCOLS;

    std::map<Window, WMWindow*> wm_windows;
    std::list<WMWindow*> wm_tops;

    Window moving;      // temporary moving WM window
    Window resizing;      // temporary resizing WM window
    XButtonEvent start_event;
    XWindowAttributes start_attrs;

    //std::map<std::pair<int, Window>, WindowManager::handler_t> handlers;
};

int main(int argc, char** argv)
{
    auto wm = WindowManager::create();
    if (!wm) {
        fprintf(stderr, "Failed to initialize window manager.");
        return EXIT_FAILURE;
    }

    wm->events_setup();
    wm->run();
    return EXIT_SUCCESS;
}

