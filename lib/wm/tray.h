#pragma once

#include <functional>
#include <memory>
#include <set>
#include <vector>

#include "object.h"
#include "widget.h"
#include "wm/selection.h"

namespace tiny::wm {

class Panel;

/**
 * @short The place of one icon in the tray: the window that embeds the icon
 * of a program (XEMBED).
 *
 * The icon is a window of the program. It is taken into this one, which is
 * as high as the panel and a bit wider than the icon. The window is given
 * back to the root window before this one goes away, so the program does not
 * lose it.
 */
class TrayIcon : public tiny::Widget {
  public:
    explicit TrayIcon(::Window client);

    ~TrayIcon() override;

    int get_border() const override { return 0; }

    //! The window of the program
    ::Window get_client() const { return client; }

    //! Take the window of the program in, tell it (`XEMBED_EMBEDDED_NOTIFY`)
    //! and show it; the place must be realized
    void embed();

    //! Give the window of the program back to the root window, hidden
    void release();

    //! The window of the program is gone or taken away: do not touch it
    void forget() { embedded = false; }

  private:
    ::Window client;
    bool embedded = false;
};

/**
 * @short The system tray of the panel (freedesktop.org System Tray
 * Protocol, XEMBED).
 *
 * The tray owns the selection `_NET_SYSTEM_TRAY_S<n>`; a program that wants
 * an icon in the panel sends `_NET_SYSTEM_TRAY_OPCODE` with the window of the
 * icon to the owner. The icons are put to the left from the other applets of
 * the panel, the newest one at the end. An icon that the program hides
 * (`_XEMBED_INFO`) takes no place, one that is destroyed or taken away by
 * somebody else is forgotten. The messages with balloon texts are ignored.
 */
class Tray : public tiny::Object {
  public:
    //! The panel must be realized and live longer than the tray
    Tray(Panel& panel, ::Window root, int screen);

    ~Tray() override;

    //! Become the system tray and tell the programs; false when there is
    //! another tray
    bool acquire();

    //! Windows that the tray must not take (the shell manages them): the
    //! windows of the manager, its docks and the windows with
    //! override-redirect are never taken anyway
    std::function<bool(::Window)> is_taken;

    //! The window the programs send their requests to
    ::Window get_window() const { return selection.get_window(); }

    //! The selection of the `SelectionClear` event is the one we hold
    bool is_lost_by(const XSelectionClearEvent& e) const
    {
        return selection.is_lost_by(e);
    }

    //! The number of the icons that are shown
    size_t size() const { return icons.size(); }

    //! Does the tray know the window (shown or hidden by the program)?
    bool has_icon(::Window client) const;

    //! The place of the icon of the window, null when it is not shown
    const TrayIcon* find(::Window client) const;

  private:
    //! The values of `_NET_SYSTEM_TRAY_OPCODE`
    enum Opcode { RequestDock = 0, BeginMessage = 1, CancelMessage = 2 };

    void on_message(const XClientMessageEvent& e);
    void dock(::Window client);
    void show(::Window client);
    void hide(::Window client);
    void forget(::Window client);
    void on_client_property(::Window client);
    void on_client_reparent(::Window client);

    //! `XEMBED_MAPPED` of `_XEMBED_INFO`; a program without it is shown
    bool wants_to_be_shown(::Window client) const;

    std::vector<std::unique_ptr<TrayIcon>>::iterator place_of(::Window client);

    Panel& panel;
    ::Display* display;
    ::Window root;
    Selection selection;
    std::vector<std::unique_ptr<TrayIcon>> icons; //!< in the order of docking
    std::set<::Window> hidden; //!< the programs that do not want to be seen
};

} // namespace tiny::wm
