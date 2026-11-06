// A program with one icon in the system tray (freedesktop.org System Tray
// Protocol and XEMBED). A click on the icon opens a menu with Quit, which ends
// the program. The icon is one of the symbolic icons of the shell, so run it
// with the data of the repository when tiny-shell is not installed:
//
//   XDG_DATA_DIRS=$PWD/data:/usr/share build/examples/tray_icon
//
// The program waits when there is no tray (a tray that starts later announces
// itself with the MANAGER message) and asks again when the tray is replaced.
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "button.h"
#include "container.h"
#include "wm/shell_theme.h"
#include "wm/symbolic_icons.h"
#include "x_util.h"
#include "xdg/application_index.h"

namespace tiny {
extern Handlers handlers;
}

namespace {

constexpr const char* icon_name = "preferences-system";

class TrayExample {
  public:
    TrayExample():
        display(tiny::get_display()),
        root(XDefaultRootWindow(display)),
        icons(display, root, tiny::xdg::default_data_dirs()),
        menu(tiny::wm::shell_theme.menu_width,
             tiny::wm::shell_theme.popup_item_height),
        quit(tiny::wm::shell_theme.menu_width,
             tiny::wm::shell_theme.popup_item_height, "Quit")
    {
        menu.realize(root);
        quit.set_text_padding(tiny::wm::shell_theme.menu_padding);
        quit.set_border(0);
        menu.push_start(&quit);
        quit.on_click.connect([this](const XEvent&) { running = false; });

        // The tray gives the icon the background of the panel
        window = XCreateSimpleWindow(display, root, 0, 0, tiny::x_dim(width),
                                     tiny::x_dim(height), 0, 0, 0);
        XSetWindowBackgroundPixmap(display, window, ParentRelative);
        XSelectInput(display, window,
                     ExposureMask | ButtonPressMask | StructureNotifyMask);
        // XEMBED_INFO: version 0, XEMBED_MAPPED (the tray maps the window)
        const long info[] = {0, 1};
        XChangeProperty(display, window, tiny::get_display()._XEMBED_INFO,
                        tiny::get_display()._XEMBED_INFO, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(info), 2);

        // A tray that starts later announces itself on the root window
        XSelectInput(display, root, StructureNotifyMask);
        request_dock();
    }

    ~TrayExample() { XDestroyWindow(display, window); }

    TrayExample(const TrayExample&) = delete;
    TrayExample& operator=(const TrayExample&) = delete;

    void main_loop()
    {
        XEvent e;
        while (running) {
            XNextEvent(display, &e);
            const ::Window target = tiny::window_of_event(e);
            if (e.type == ClientMessage && target == root) {
                on_root_message(e.xclient);
            } else if (target == window) {
                on_icon_event(e);
            } else if (tiny::handlers.count({e.type, target})) {
                tiny::handlers.call_handler({e.type, target}, e); // the menu
            }
        }
    }

  private:
    Atom tray_selection() const
    {
        const std::string name =
                "_NET_SYSTEM_TRAY_S" + std::to_string(XDefaultScreen(display));
        return XInternAtom(display, name.c_str(), False);
    }

    //! Ask the owner of the tray selection to embed the window
    void request_dock()
    {
        const ::Window tray = XGetSelectionOwner(display, tray_selection());
        if (tray == None) {
            fprintf(stderr, "tray_icon: there is no system tray, waiting\n");
            return;
        }
        XClientMessageEvent e = {};
        e.type = ClientMessage;
        e.window = tray;
        e.message_type = tiny::get_display()._NET_SYSTEM_TRAY_OPCODE;
        e.format = 32;
        e.data.l[0] = CurrentTime;
        e.data.l[1] = 0; // SYSTEM_TRAY_REQUEST_DOCK
        e.data.l[2] = static_cast<long>(window);
        XSendEvent(display, tray, False, NoEventMask,
                   reinterpret_cast<XEvent*>(&e));
        XFlush(display);
    }

    void on_root_message(const XClientMessageEvent& e)
    {
        if (e.message_type == tiny::get_display().MANAGER &&
            static_cast<Atom>(e.data.l[1]) == tray_selection()) {
            request_dock();
        }
    }

    void on_icon_event(const XEvent& e)
    {
        switch (e.type) {
            case ConfigureNotify: // the tray decides the size
                width = e.xconfigure.width;
                height = e.xconfigure.height;
                break;
            case Expose:
                if (e.xexpose.count == 0) {
                    draw();
                }
                break;
            case ButtonPress:
                if (e.xbutton.button == Button1 ||
                    e.xbutton.button == Button3) {
                    // under the icon, which is at the top of the screen
                    menu.popup(e.xbutton.x_root,
                               e.xbutton.y_root - e.xbutton.y + height);
                }
                break;
            default:
                break;
        }
    }

    void draw()
    {
        XClearWindow(display, window);
        const unsigned long color = tiny::theme.widget.get_fg(0);
        if (!icons.draw(window, icon_name, std::min(width, height), color,
                        width, height)) { // the files are not installed
            XSetForeground(display, DefaultGC(display, 0), color);
            XFillRectangle(display, window, DefaultGC(display, 0), width / 4,
                           height / 4, tiny::x_dim(width / 2),
                           tiny::x_dim(height / 2));
        }
        XFlush(display);
    }

    ::Display* display;
    ::Window root;
    ::Window window = None;
    // until the tray says how big the icon is
    int width = tiny::wm::shell_theme.tray_icon;
    int height = tiny::wm::shell_theme.tray_icon;
    tiny::wm::SymbolicIcons icons;
    // the item is destroyed before the menu that has its window
    tiny::Popover menu;
    tiny::LabelButton quit;
    bool running = true;
};

} // namespace

int main()
{
    tiny::Display::init();
    tiny::wm::shell_theme.init();
    {
        TrayExample example;
        example.main_loop();
    }
    // The static styles close their fonts after the display is closed, which
    // crashes (see "SIGSEGV at exit" in TODO): leave without them.
    std::fflush(nullptr);
    _exit(0);
}
