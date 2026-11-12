# Comparison

What tiny-shell has next to other window managers and desktops for X11. The
columns are the window managers on their own and, where a window manager is
part of a desktop, the desktop: **LXDE** is Openbox with LXPanel and PCManFM,
**Xfce** is xfwm4 with the Xfce panel and the rest, **GNOME** is Mutter and
GNOME Shell.

Legend: ✓ the project has it, ✗ it does not (it may be possible with an
external program), ◔ it is planned for tiny-shell (see the
[roadmap](roadmap.md)), ? it is not decided yet: it may come one day, it may
not.

This is a comparison of what is shipped and enabled by default, made from the
documentation of the projects. It is not a benchmark and no judgement of
quality: tiny-shell is deliberately small and does less than a desktop. If a
cell is wrong, please
[open an issue](https://github.com/ondratu/tiny-shell/issues) or a pull request.

## Windows

| Feature | tiny-shell | Openbox | Blackbox | Fluxbox | LXDE | Xfce | GNOME |
| ------- | :--: | :--: | :--: | :--: | :--: | :--: | :--: |
| Reparenting frames with a title bar | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Minimize, maximize and close buttons | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Shade (roll up to the title bar) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ |
| Window menu | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Window switcher (Alt+Tab) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Live thumbnails of windows | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Always on top | ✓ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Move with Alt + drag | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Snapping to the screen edges | ◔ | ✗ | ✓ | ✓ | ✗ | ✓ | ✓ |
| Half-screen maximize | ◔ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Focus follows mouse | ? | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Virtual desktops | ◔ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Minimize animation | ◔ | ✓ | ✗ | ✗ | ✓ | ✗ | ✓ |
| Compositor (transparency, shadows) | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Configurable key bindings | ◔ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Themes made by the user | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Plugins or scripting | ◔ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |

tiny-shell has no live thumbnails of windows, and it is a choice: they need a
compositor or a copy of the window contents (see the [list of what was left
out](https://github.com/ondratu/tiny-shell/blob/master/wont.md)). In Xfce they
work only with its compositor turned on.

## Standards

| Feature | tiny-shell | Openbox | Blackbox | Fluxbox | LXDE | Xfce | GNOME |
| ------- | :--: | :--: | :--: | :--: | :--: | :--: | :--: |
| ICCCM | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| EWMH | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Resize without tearing (`_NET_WM_SYNC_REQUEST`) | ✓ | ✓ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Finds applications that do not respond (`_NET_WM_PING`) | ✓ | ✓ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Startup notification | ✓ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| Desktop Entry and icon themes | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| XSETTINGS for GTK and Qt programs | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| System tray of the XEMBED protocol | ✓ | ✗ | ✗ | ✓ | ✓ | ✓ | ✗ |
| StatusNotifierItem tray (modern applications) | ◔ | ✗ | ✗ | ✗ | ✗ | ✓ | ✗ |
| Notifications of D-Bus | ◔ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Several monitors with RandR | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Scaling for high-DPI screens | ✓ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Wayland session | ✗ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |

GNOME Shell has no tray of its own: the XEMBED tray was removed years ago and
the StatusNotifierItem tray comes from an extension (AppIndicator), which some
distributions (Ubuntu, for one) install and enable by default. The two rows of
the trays count what GNOME ships itself, so both are ✗ for it.

GNOME has been phasing out its X11 session. The X11 cells of the GNOME column
describe the last releases that had it.

## Shell

| Feature | tiny-shell | Openbox | Blackbox | Fluxbox | LXDE | Xfce | GNOME |
| ------- | :--: | :--: | :--: | :--: | :--: | :--: | :--: |
| Panel | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Dock with running applications and favorites | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |
| Task bar (list of windows in the panel) | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ | ✗ |
| Menu of applications by category | ◔ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Launcher with search | ✓ | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Run dialog | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Clock | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Wallpaper | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Settings application | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Keyboard and mouse settings | ◔ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Logout, restart and shutdown dialog | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Autostart of applications (XDG Autostart) | ✓ | ✓ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Saving and restoring the session (XSMP) | ? | ✓ | ✗ | ✗ | ✓ | ✓ | ✗ |
| Autostart settings | ◔ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Default applications settings | ◔ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Screenshot tool | ? | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| Screen lock | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Power saving of the display (DPMS) | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
| Accessibility settings (sticky keys, high contrast) | ? | ✗ | ✗ | ✗ | ✗ | ✓ | ✓ |
| File manager | ? | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |

## Session

The **autostart** row is the XDG Autostart specification: the `.desktop` files in
`~/.config/autostart` and `/etc/xdg/autostart` start with the session. Openbox does
it with its `openbox-session` script; Blackbox and Fluxbox leave it to a
`startup` script of the user. See [The session](session.md).

The **XSMP** row is the X Session Management Protocol (a layer over ICE, from
the X Consortium): a session manager asks the programs to save their state when the
user logs out and starts them again, with that state, at the next login. A window
manager takes part by saving what it knows about its windows. Openbox
(a client of the manager, `--sm-disable` turns it off) and Xfce (`xfce4-session`
with `xfwm4`) do; KDE does with `ksmserver`; Window Maker has a session
saving of its own; Blackbox and Fluxbox keep their settings only. GNOME had it
and removed XSMP and the restoring of the session in GNOME 49. macOS restores
the windows with the setting "Reopen windows when logging back in", not with
XSMP.

The reason for the **?** of tiny-shell is how much the protocol is used today.
Many current programs do not use it (a browser restores its windows by itself),
so the shell would restore only a part of the session. It may be useful to someone
(and it is a small extra with the library `libSM`, optional like the other
libraries), so it is not ruled out, but there is no plan either. Logout does not
need it: it asks the programs to close, so that they ask for the unsaved work (see
[Ending the session](session.md#ending-the-session)).

## Other libraries

**tiny-shell supports other libraries, but it does not require them.** It
builds and runs with libX11 and a few light X libraries; the rest is switched on
where it is installed. A tick means that the project has the support, whether
the library is optional or required there is not compared. For the window
managers the usual companions count too (Openbox: `obconf`, which is GTK;
Fluxbox: `fbsetbg`; Blackbox: `bsetroot`), but not the panels and lockers that
people add by their own choice.

| Library | tiny-shell | Openbox | Blackbox | Fluxbox | LXDE | Xfce | GNOME |
| ------- | :--: | :--: | :--: | :--: | :--: | :--: | :--: |
| D-Bus | ◔ | ✓ | ✗ | ✓ | ✓ | ✓ | ✓ |
| PAM | ✓ | ✗ | ✗ | ✗ | ✓ | ✓ | ✓ |
