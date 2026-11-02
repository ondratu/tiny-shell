// Plain Xlib program to try how a window manager treats dialogs.
//
//  - "Transient dialog" sets only WM_TRANSIENT_FOR (ICCCM 4.1.2.6): a short
//    lived window that belongs to the main window. The manager keeps it over
//    the main window and minimizes it with the main window, but the main
//    window can still get the focus.
//  - "Modal dialog" sets WM_TRANSIENT_FOR, _NET_WM_WINDOW_TYPE_DIALOG and the
//    _NET_WM_STATE_MODAL state (EWMH): the dialog is modal for the main
//    window, so the focus must stay in the dialog until it is closed.
//
// Close a dialog with its close button or by a click into it.
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cstdio>
#include <cstring>
#include <map>

namespace {

constexpr unsigned MAIN_WIDTH = 320;
constexpr unsigned MAIN_HEIGHT = 120;
constexpr unsigned DIALOG_WIDTH = 240;
constexpr unsigned DIALOG_HEIGHT = 100;
constexpr int BUTTON_X = 20;
constexpr int BUTTON_WIDTH = 280;
constexpr int BUTTON_HEIGHT = 36;
constexpr int TRANSIENT_Y = 15;
constexpr int MODAL_Y = 65;

class Dialogs {
  public:
    Dialogs()
    {
        display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            fprintf(stderr, "Can't open the display\n");
            return;
        }
        delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
        main_window = create("Dialogs", MAIN_WIDTH, MAIN_HEIGHT);
        XMapWindow(display, main_window);
    }

    Dialogs(const Dialogs&) = delete;
    Dialogs& operator=(const Dialogs&) = delete;

    ~Dialogs()
    {
        if (display != nullptr) {
            XCloseDisplay(display);
        }
    }

    bool opened() const { return display != nullptr; }

    void run()
    {
        while (main_window != None) {
            XEvent e;
            XNextEvent(display, &e);
            switch (e.type) {
                case Expose:
                    redraw(e.xexpose.window);
                    break;
                case ButtonPress:
                    on_button(e.xbutton);
                    break;
                case ClientMessage:
                    on_client_message(e.xclient);
                    break;
                default:
                    break;
            }
        }
    }

  private:
    ::Window create(const char* title, unsigned width, unsigned height)
    {
        const int screen = DefaultScreen(display);
        const ::Window window = XCreateSimpleWindow(
                display, RootWindow(display, screen), 0, 0, width, height, 0,
                BlackPixel(display, screen), WhitePixel(display, screen));
        XSelectInput(display, window, ExposureMask | ButtonPressMask);
        XStoreName(display, window, title);
        XSetWMProtocols(display, window, &delete_window, 1);
        XClassHint class_hint = {const_cast<char*>("dialogs"),
                                 const_cast<char*>("Dialogs")};
        XSetClassHint(display, window, &class_hint);
        windows[window] = title;
        return window;
    }

    // a dialog of the main window; `modal` adds what EWMH asks for
    void open_dialog(bool modal)
    {
        const ::Window dialog =
                create(modal ? "Modal dialog" : "Transient dialog",
                       DIALOG_WIDTH, DIALOG_HEIGHT);
        // ICCCM: the dialog belongs to the main window
        XSetTransientForHint(display, dialog, main_window);
        // the group lets the manager find the other windows of the program
        XWMHints hints = {};
        hints.flags = InputHint | WindowGroupHint;
        hints.input = True;
        hints.window_group = main_window;
        XSetWMHints(display, dialog, &hints);

        if (modal) {
            const Atom type_atom =
                    XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
            const Atom type =
                    XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
            XChangeProperty(display, dialog, type_atom, XA_ATOM, 32,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(&type), 1);
            // the state is set before the window is mapped; later the
            // client asks with a ClientMessage to the root window
            const Atom state_atom =
                    XInternAtom(display, "_NET_WM_STATE", False);
            const Atom state =
                    XInternAtom(display, "_NET_WM_STATE_MODAL", False);
            XChangeProperty(display, dialog, state_atom, XA_ATOM, 32,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(&state), 1);
        }
        XMapWindow(display, dialog);
    }

    void redraw(::Window window)
    {
        const auto found = windows.find(window);
        if (found == windows.end()) {
            return;
        }
        const int screen = DefaultScreen(display);
        GC gc = XCreateGC(display, window, 0, nullptr);
        XSetForeground(display, gc, BlackPixel(display, screen));
        if (window == main_window) {
            draw_button(window, gc, TRANSIENT_Y,
                        "Transient dialog (WM_TRANSIENT_FOR)");
            draw_button(window, gc, MODAL_Y,
                        "Modal dialog (+ _NET_WM_STATE_MODAL)");
        } else {
            XDrawString(display, window, gc, 20, 40, found->second,
                        static_cast<int>(strlen(found->second)));
            XDrawString(display, window, gc, 20, 70, "Click to close", 14);
        }
        XFreeGC(display, gc);
    }

    void draw_button(::Window window, GC gc, int y, const char* text)
    {
        XDrawRectangle(display, window, gc, BUTTON_X, y, BUTTON_WIDTH,
                       BUTTON_HEIGHT);
        XDrawString(display, window, gc, BUTTON_X + 10, y + 23, text,
                    static_cast<int>(strlen(text)));
    }

    void on_button(const XButtonEvent& e)
    {
        if (e.window != main_window) {
            close(e.window); // a click into a dialog closes it
            return;
        }
        if (e.x < BUTTON_X || e.x > BUTTON_X + BUTTON_WIDTH) {
            return;
        }
        if (e.y >= TRANSIENT_Y && e.y <= TRANSIENT_Y + BUTTON_HEIGHT) {
            open_dialog(false);
        } else if (e.y >= MODAL_Y && e.y <= MODAL_Y + BUTTON_HEIGHT) {
            open_dialog(true);
        }
    }

    void on_client_message(const XClientMessageEvent& e)
    {
        if (static_cast<Atom>(e.data.l[0]) == delete_window) {
            close(e.window);
        }
    }

    void close(::Window window)
    {
        if (window == main_window) {
            main_window = None; // ends the loop
        }
        windows.erase(window);
        XDestroyWindow(display, window);
    }

    Display* display = nullptr;
    Atom delete_window = None;
    ::Window main_window = None;
    std::map<::Window, const char*> windows; //!< window -> its title
};

} // namespace

int main()
{
    Dialogs dialogs;
    if (!dialogs.opened()) {
        return 1;
    }
    dialogs.run();
    return 0;
}
