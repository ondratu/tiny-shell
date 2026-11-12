# Roadmap

What is planned, grouped by importance. What a window manager must do for other
programs is done, only small details are left. The detailed list with the
reasons is in the file
[TODO](https://github.com/ondratu/tiny-shell/blob/master/TODO) of the
repository, the D-Bus plan is in
[TODO-DBUS.md](https://github.com/ondratu/tiny-shell/blob/master/TODO-DBUS.md).
What is not decided yet is in the
[backlog](https://github.com/ondratu/tiny-shell/blob/master/backlog.md), and
what was left out on purpose, with the reason, is in
[wont.md](https://github.com/ondratu/tiny-shell/blob/master/wont.md).

## Should: what people expect from a desktop

- Applications of the launcher grouped by category, like the Applications Menu
  extension of GNOME Shell
- More in the configuration file: key bindings, panel layout, plugins
- No flicker: double buffered drawing of the panel and the dock
- Optional D-Bus (libdbus-1): the base, the icons of modern applications
  (StatusNotifierItem) and the activation of applications

## Could: nice to have

- Virtual desktops (there is one desktop now), with moving windows between
  them like GNOME
- Plugins and scripting in Lua, as an optional build feature
- Window snapping to the screen edges and half-screen maximize
- Screen lock: lock before the computer sleeps (needs logind, see
  TODO-DBUS.md), a switch of the user from the lock screen
- More in the settings: keyboard and mouse, the autostart programs, default
  applications
- A simple animation of minimizing, which can be turned off in the settings

## Not decided

These may come one day, or may not. They are marked with `?` in the
[comparison](comparison.md).

- Focus follows mouse
- Saving and restoring the session (XSMP), see the [comparison](comparison.md#session)
- Accessibility
- A very small file manager, a screenshot tool
