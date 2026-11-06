#include "wm/tray.h"

#include <X11/Xatom.h>

#include <algorithm>
#include <chrono>

#include "wm/panel.h"
#include "wm/shell_theme.h"
#include "x_util.h"

namespace tiny::wm {

namespace {

constexpr long xembed_embedded_notify = 0;
constexpr unsigned long xembed_mapped = 1;

int ignore_error(::Display* /*display*/, XErrorEvent* /*error*/)
{
    return 0;
}

/**
 * @short The requests on a window of a program may fail, the program can be
 * gone at any moment. The errors of the requests made while this lives are
 * ignored.
 */
class IgnoreErrors {
  public:
    explicit IgnoreErrors(::Display* display_arg): display(display_arg)
    {
        XSync(display, False); // the older errors are not ours to ignore
        old_handler = XSetErrorHandler(&ignore_error);
    }

    ~IgnoreErrors()
    {
        XSync(display, False);
        XSetErrorHandler(old_handler);
    }

    IgnoreErrors(const IgnoreErrors&) = delete;
    IgnoreErrors& operator=(const IgnoreErrors&) = delete;

  private:
    ::Display* display;
    XErrorHandler old_handler;
};

} // namespace

TrayIcon::TrayIcon(::Window client_arg):
    tiny::Widget(shell_theme.tray_icon + 2 * shell_theme.tray_gap,
                 shell_theme.panel),
    client(client_arg)
{
    name = "wm_tray_icon";
}

TrayIcon::~TrayIcon()
{
    release(); // destroying this window would destroy the one of the program
}

void TrayIcon::embed()
{
    if (!is_realized || embedded) {
        return;
    }
    const IgnoreErrors guard(display);
    const int icon = shell_theme.tray_icon;
    XResizeWindow(display, client, x_dim(icon), x_dim(icon));
    XReparentWindow(display, client, window, (width - icon) / 2,
                    (height - icon) / 2);
    embedded = true;

    XClientMessageEvent message = {};
    message.type = ClientMessage;
    message.window = client;
    message.message_type = display._XEMBED;
    message.format = 32;
    message.data.l[0] = CurrentTime;
    message.data.l[1] = xembed_embedded_notify;
    message.data.l[3] = static_cast<long>(window);
    XSendEvent(display, client, False, NoEventMask,
               reinterpret_cast<XEvent*>(&message));
    XMapWindow(display, client);
}

void TrayIcon::release()
{
    if (!embedded) {
        return;
    }
    embedded = false;
    const IgnoreErrors guard(display);
    XUnmapWindow(display, client);
    XReparentWindow(display, client, XDefaultRootWindow(display), 0, 0);
}

Tray::Tray(Panel& panel_arg, ::Window root_arg, int screen):
    panel(panel_arg),
    display(tiny::get_display()),
    root(root_arg),
    selection(display, root_arg, tray_selection_name(screen))
{}

Tray::~Tray()
{
    for (const auto& icon : icons) { // the programs get their windows back
        icon->release();
        panel.remove(icon.get());
    }
}

bool Tray::acquire()
{
    const long horizontal = 0;
    XChangeProperty(display, selection.get_window(),
                    tiny::get_display()._NET_SYSTEM_TRAY_ORIENTATION,
                    XA_CARDINAL, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(&horizontal), 1);
    if (selection.acquire(false, std::chrono::milliseconds(0)) !=
        Selection::Result::Acquired) {
        return false;
    }
    connect_window(ClientMessage, selection.get_window(),
                   [this](const XEvent& e) { on_message(e.xclient); });
    selection.announce();
    return true;
}

bool Tray::has_icon(::Window client) const
{
    return find(client) != nullptr || hidden.count(client) > 0;
}

const TrayIcon* Tray::find(::Window client) const
{
    for (const auto& icon : icons) {
        if (icon->get_client() == client) {
            return icon.get();
        }
    }
    return nullptr;
}

std::vector<std::unique_ptr<TrayIcon>>::iterator Tray::place_of(::Window client)
{
    return std::find_if(icons.begin(), icons.end(), [client](const auto& icon) {
        return icon->get_client() == client;
    });
}

void Tray::on_message(const XClientMessageEvent& e)
{
    if (e.message_type != tiny::get_display()._NET_SYSTEM_TRAY_OPCODE ||
        e.format != 32) {
        return;
    }
    if (e.data.l[1] == RequestDock) {
        dock(static_cast<::Window>(e.data.l[2]));
    }
    // the balloon messages (BeginMessage, CancelMessage) are not shown
}

void Tray::dock(::Window client)
{
    if (client == None || client == root || client == get_window() ||
        has_icon(client)) {
        return;
    }
    {
        const IgnoreErrors guard(display);
        XWindowAttributes attrs;
        if (!XGetWindowAttributes(display, client, &attrs)) {
            return; // gone already
        }
        XSelectInput(display, client, StructureNotifyMask | PropertyChangeMask);
    }
    connect_window(DestroyNotify, client,
                   [this, client](const XEvent&) { forget(client); });
    connect_window(ReparentNotify, client, [this, client](const XEvent&) {
        on_client_reparent(client);
    });
    connect_window(PropertyNotify, client, [this, client](const XEvent& e) {
        if (e.xproperty.atom == tiny::get_display()._XEMBED_INFO) {
            on_client_property(client);
        }
    });
    if (wants_to_be_shown(client)) {
        show(client);
    } else {
        hidden.insert(client);
    }
}

void Tray::show(::Window client)
{
    hidden.erase(client);
    icons.push_back(std::make_unique<TrayIcon>(client));
    panel.add_applet(icons.back().get());
    icons.back()->embed();
}

void Tray::hide(::Window client)
{
    const auto it = place_of(client);
    if (it == icons.end()) {
        return;
    }
    (*it)->release();
    panel.remove(it->get());
    icons.erase(it);
    hidden.insert(client);
}

void Tray::forget(::Window client)
{
    const auto it = place_of(client);
    if (it != icons.end()) {
        (*it)->forget();
        panel.remove(it->get());
        icons.erase(it);
    }
    hidden.erase(client);
    disconnect_window(DestroyNotify, client);
    disconnect_window(ReparentNotify, client);
    disconnect_window(PropertyNotify, client);
}

void Tray::on_client_property(::Window client)
{
    if (!has_icon(client)) {
        return;
    }
    const bool shown = find(client) != nullptr;
    const bool wanted = wants_to_be_shown(client);
    if (wanted && !shown) {
        show(client);
    } else if (!wanted && shown) {
        hide(client);
    }
}

void Tray::on_client_reparent(::Window client)
{
    const auto it = place_of(client);
    if (it == icons.end()) {
        return; // hidden: it is at the root window, where we put it
    }
    // The event can be an old one (the program was hidden and shown again),
    // so the server says where the window is.
    ::Window parent = None;
    {
        const IgnoreErrors guard(display);
        ::Window the_root;
        ::Window* children = nullptr;
        unsigned count = 0;
        if (XQueryTree(display, client, &the_root, &parent, &children,
                       &count)) {
            if (children) {
                XFree(children);
            }
        } else {
            parent = None;
        }
    }
    if (parent != (*it)->get_window()) { // taken away by somebody else
        forget(client);
    }
}

bool Tray::wants_to_be_shown(::Window client) const
{
    const IgnoreErrors guard(display);
    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    const Atom property = tiny::get_display()._XEMBED_INFO;
    const int status = XGetWindowProperty(display, client, property, 0, 2,
                                          False, AnyPropertyType, &type,
                                          &format, &count, &left, &data);
    bool shown = true; // a program without the property is shown
    if (status == Success && data != nullptr) {
        if (format == 32 && count >= 2) {
            shown = (reinterpret_cast<unsigned long*>(data)[1] &
                     xembed_mapped) != 0;
        }
        XFree(data);
    }
    return shown;
}

} // namespace tiny::wm
