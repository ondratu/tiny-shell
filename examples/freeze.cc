// Plain Xlib program to try how a window manager treats an application that
// does not respond.
//
// The button freezes the program for 30 seconds: it does not read its events,
// so it does not answer `_NET_WM_PING` (EWMH) either. Watch the title bar
// ("not responding"), try to close the window and choose "Wait" or
// "Force quit" in the dialog of the manager. The program tells its process id
// to `_NET_WM_PID`, so the manager can kill it. When the freeze ends before
// that, the title comes back and the dialog goes away.
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <unistd.h>

#include <climits>
#include <cstdio>
#include <cstring>

namespace {

constexpr unsigned WIDTH = 320;
constexpr unsigned HEIGHT = 120;
constexpr int BUTTON_X = 20;
constexpr int BUTTON_Y = 40;
constexpr int BUTTON_WIDTH = 280;
constexpr int BUTTON_HEIGHT = 36;
constexpr unsigned FREEZE_SECONDS = 30;

class Freeze {
  public:
    Freeze()
    {
        display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            fprintf(stderr, "Can't open the display\n");
            return;
        }
        const int screen = DefaultScreen(display);
        window = XCreateSimpleWindow(
                display, RootWindow(display, screen), 0, 0, WIDTH, HEIGHT, 0,
                BlackPixel(display, screen), WhitePixel(display, screen));
        XSelectInput(display, window, ExposureMask | ButtonPressMask);
        XStoreName(display, window, "Freeze");
        XClassHint class_hint = {const_cast<char*>("freeze"),
                                 const_cast<char*>("Freeze")};
        XSetClassHint(display, window, &class_hint);

        delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
        ping = XInternAtom(display, "_NET_WM_PING", False);
        Atom protocols[] = {delete_window, ping};
        XSetWMProtocols(display, window, protocols, 2);

        // what the manager needs to kill the process of a frozen program
        const long pid = getpid();
        XChangeProperty(display, window,
                        XInternAtom(display, "_NET_WM_PID", False), XA_CARDINAL,
                        32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(&pid), 1);
        char host[256] = {};
        gethostname(host, sizeof(host) - 1);
        char* names[] = {host};
        XTextProperty machine;
        if (XStringListToTextProperty(names, 1, &machine)) {
            XSetWMClientMachine(display, window, &machine);
            XFree(machine.value);
        }
        XMapWindow(display, window);
    }

    Freeze(const Freeze&) = delete;
    Freeze& operator=(const Freeze&) = delete;

    ~Freeze()
    {
        if (display != nullptr) {
            XCloseDisplay(display);
        }
    }

    bool opened() const { return display != nullptr; }

    void run()
    {
        while (running) {
            XEvent e;
            XNextEvent(display, &e);
            switch (e.type) {
                case Expose:
                    redraw();
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
    void redraw()
    {
        XClearWindow(display, window);
        GC gc = XCreateGC(display, window, 0, nullptr);
        XSetForeground(display, gc,
                       BlackPixel(display, DefaultScreen(display)));
        XDrawString(display, window, gc, BUTTON_X, 25, status,
                    static_cast<int>(strlen(status)));
        XDrawRectangle(display, window, gc, BUTTON_X, BUTTON_Y, BUTTON_WIDTH,
                       BUTTON_HEIGHT);
        const char* text = "Freeze for 30 seconds";
        XDrawString(display, window, gc, BUTTON_X + 10, BUTTON_Y + 23, text,
                    static_cast<int>(strlen(text)));
        XFreeGC(display, gc);
    }

    void on_button(const XButtonEvent& e)
    {
        if (e.x < BUTTON_X || e.x > BUTTON_X + BUTTON_WIDTH || e.y < BUTTON_Y ||
            e.y > BUTTON_Y + BUTTON_HEIGHT) {
            return;
        }
        // say it before the freeze, nothing is drawn meanwhile
        status = "Freezing...";
        redraw();
        XFlush(display);
        // no event is read meanwhile: no ping is answered
        sleep(FREEZE_SECONDS);
        status = "Alive again";
        redraw();
    }

    void on_client_message(const XClientMessageEvent& e)
    {
        const auto protocol = static_cast<Atom>(e.data.l[0]);
        if (protocol == delete_window) {
            running = false;
        } else if (protocol == ping) {
            // EWMH: send the message back to the root window
            XClientMessageEvent reply = e;
            reply.window = DefaultRootWindow(display);
            XSendEvent(display, reply.window, False,
                       SubstructureNotifyMask | SubstructureRedirectMask,
                       reinterpret_cast<XEvent*>(&reply));
        }
    }

    Display* display = nullptr;
    ::Window window = None;
    Atom delete_window = None;
    Atom ping = None;
    const char* status = "Press the button, then try to close the window";
    bool running = true;
};

} // namespace

int main()
{
    Freeze freeze;
    if (!freeze.opened()) {
        return 1;
    }
    freeze.run();
    return 0;
}
