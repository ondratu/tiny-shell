# TODO-DBUS

Plan for the communication of the shell with applications: the missing X11
properties (ICCCM, EWMH, XSETTINGS) and the optional D-Bus interface. The short
items with priorities are in todo.txt and point here; this file keeps the
reasons and the details. The rules are in CONTRIBUTING.md.

D-Bus does not replace ICCCM and EWMH. They stay the base of the communication
between a window and the window manager. D-Bus adds what X11 does not have or
does badly: services of the session (sleep, lock, inhibit), notifications,
status icons of modern applications and activation of applications.

## Order of work

1. **The base of D-Bus.** The connection, the integration into the event loop,
   the optional build (see below), a fake bus for the tests.
2. **logind** (`org.freedesktop.login1`): `PrepareForSleep`, `Lock`/`Unlock`,
   `Inhibit`, and `PowerOff`/`Reboot`/`Suspend` from the menu without `sudo`.
   It is small and useful at once.
3. **`org.freedesktop.ScreenSaver` and `org.freedesktop.Notifications`.** They
   are independent. Notifications is the bigger one (its own window, drawing,
   timers).
4. **StatusNotifierWatcher** (and `com.canonical.dbusmenu`). Makes the tray
   usable for modern applications.
5. **`org.freedesktop.Application` activation** in the launcher and the dock
   (`DBusActivatable=true`).
6. **`org.freedesktop.portal.Settings`** (`color-scheme`) after the switching of
   themes is done.

Items 1, 4 and 5 are in todo.txt. The others wait until they are needed.

## Build and dependencies

The library is **libdbus-1** (the reference C library; a small event loop
adapter with `DBusWatch` and `DBusTimeout` on top of the `poll` the shell
already uses). sd-bus needs libsystemd (not on every system), GDBus pulls in
GLib, sdbus-c++ pulls in sd-bus; none of them fits.

- A make variable `DBUS`: `auto` (the default), `yes`, `no`.
  - `auto`: `pkg-config --exists dbus-1` decides. If it is found, the flags and
    libraries are added and `-DTINY_HAVE_DBUS` is defined. If it is not found,
    the build goes on without D-Bus and **prints a notice** that the features
    that need it are compiled out (status icons of modern applications,
    notifications, logind). The notice must not be lost in the build output.
  - `yes`: D-Bus is required, a missing libdbus-1 stops the build with a message.
  - `no`: no D-Bus and no notice.
- `make help` lists the variable; the documentation (doc/install.md) lists the optional dependency and
  the package names (`libdbus-1-dev`, `dbus-devel`, ...).
- At run time the shell does not need the bus: when `DBUS_SESSION_BUS_ADDRESS`
  is not set or the connection fails, it goes on without it and logs one line.
  A bus that disappears later is not fatal either.
- The code lives behind one small interface (`tiny::dbus`: connection, service
  that exports methods and signals, proxy that calls them). `lib/wm` does not
  include `<dbus/dbus.h>`; with `DBUS=no` only an empty implementation is
  built. The tests use a fake bus, so they do not need a daemon.
- **CONTRIBUTING.md and doc/developers/ have to change with it.** Goal 4
  (CONTRIBUTING.md and doc/developers/index.md) says "No D-Bus ... required.
  Everything extra is optional and off by default", and doc/developers/dependencies.md
  says "Everything else needs a discussion first". A build that finds libdbus-1
  by itself is optional at build time but not off by default, so update both in
  the same change that adds `DBUS`.

## D-Bus services

### Session and the system (logind, ScreenSaver)
- `org.freedesktop.login1` (the system bus): `PrepareForSleep` (lock before
  sleep), `Lock`/`Unlock` signals, `Inhibit` (do not sleep while something is
  playing), the methods `PowerOff`, `Reboot`, `Suspend` for the menu. This
  solves "reboot and poweroff in the form" of tiny-settings without setuid.
- `org.freedesktop.ScreenSaver` and `org.gnome.SessionManager.Inhibit`: video
  players and browsers ask not to blank the screen. Without the service it does
  not work.
- Register the session (`XDG_SESSION_ID`), end it properly.

### Notifications (`org.freedesktop.Notifications`)
- The shell is the notification daemon: `Notify`, `CloseNotification`,
  `GetCapabilities`, `GetServerInformation`, the signals `ActionInvoked` and
  `NotificationClosed`. Actions (buttons), urgency, icon, expire timeout.
- It also replaces the balloon texts of the XEmbed tray (`BEGIN_MESSAGE`):
  modern applications do not use them.
- A window with its own drawing and timers, so it is a big separate feature.

### Status icons (`org.kde.StatusNotifierWatcher`)
- Most modern applications (Electron, Qt, GTK with libappindicator, Telegram,
  Discord, Steam) use StatusNotifierItem and not the XEmbed tray. Our tray does
  not see them.
- The shell implements `StatusNotifierWatcher` and a host. The icon comes as a
  pixmap or by name (the icon theme is in the launcher), the menu by
  `com.canonical.dbusmenu`. Likely the most important D-Bus feature for the tray.

### Activation of applications (`org.freedesktop.Application`)
- Applications with `DBusActivatable=true` in the desktop file are started by
  `Activate`, `Open` and `ActivateAction` and not by `Exec`. Without it such
  applications start twice or not at all. Matters for the launcher and the dock.
- `org.freedesktop.FileManager1.ShowItems` for "Show in folder".

### Settings and themes
- `org.freedesktop.portal.Settings`: applications read `color-scheme`
  (dark/light). The theme toggle then reaches GTK4 and Electron, which plain X11
  cannot do. Backends of the other portals (screenshot, file chooser) are not the
  business of a window manager.

### Panel applets (later, with the Lua plugins)
- MPRIS (`org.mpris.MediaPlayer2.*`) as a client: the player in the panel,
  Play/Pause/Next and the media keys.
- `org.freedesktop.UPower` (battery), `org.freedesktop.NetworkManager`,
  `org.bluez`: clients for applets. This is a panel, not a window manager.

## X11 properties

Found by comparing the code with EWMH, ICCCM and the common extensions. What
is not implemented yet:

| Property | Why it waits |
|----------|--------------|
| the dock marks a starting application | the startup notification has the data (name, icon, `WMCLASS`), the dock does not draw it |

Not needed, because the shell is not a compositor: `_NET_WM_BYPASS_COMPOSITOR`
and `_NET_WM_OPAQUE_REGION` are hints for a compositor that runs next to the
shell, which reads them from the client windows itself (see wont.md).

Every new property needs a test (CONTRIBUTING.md) and a line in
`_NET_SUPPORTED` when it is a property that the shell announces.
