# Architecture



- **Layers.** The toolkit (`tiny::`) knows nothing about window management.
  The window management core (managing client windows, ICCCM, EWMH) does not
  depend on the panel, the dock or the launcher. The shell (panel, dock,
  menus, launcher) uses the core through a small interface, so that it can
  be replaced or moved to another process later.
- **One event loop.** All work runs in the single `poll()` based loop of the
  manager: X events, timers (clock) and child processes. Never block it:
  no sleeping, no waiting for a process, no slow file reading in a handler.
- **Event handlers.** New code connects events with `std::function`
  (a lambda capturing `this`), registered and unregistered by RAII.
  Do not add new casts between unrelated pointer-to-member types.
- **No global state in new code.** The display, the themes (`tiny::theme`,
  `wm::shell_theme`) and the handler table are global today; new code takes what
  it needs as an argument (like `LauncherLayout::compute()` takes the theme),
  so that it can be tested and the globals can be removed later.
- **Actions have names.** What a menu item, a key or a script can ask for is
  registered by name in the `ActionRegistry` of the manager ("logout",
  "run-dialog", "theme-toggle"); do not connect a new menu item to a private
  handler of the manager. A plugin gets a panel slot from `Panel::add_applet()`.
- **Two looks, same sizes.** The theme has the variants `Dark` (the default)
  and `Light`; a color belongs to `Style::colorize()`, never to a constructor
  or a widget, so that the variant can change while the shell runs
  (`Widget::restyle()`).
- **Quiet by default.** A message for developers ("unhandled property") is
  written with `TINY_LOG()`, which prints only in a `make DEBUG=1` build. A
  problem the user can act on goes to `tiny::error()`.
- **Standards are data.** Atoms, window types and states are listed in one
  place (`display.h`). Do not spread `XInternAtom` calls over the code.
- **Decide without X when you can.** What can be decided without a display
  (which buttons a window has, the order of Alt+Tab, parsing a `.desktop`
  file, a search match, a layout computation) belongs in a plain function
  or class with unit tests. Handlers only connect it to X.
- **Be a good X11 citizen.** Check for errors of requests on windows that may
  be already gone (`BadWindow`), grab the server only for short, bounded
  work, and release grabs. Never leave a client window unreachable when the
  window manager exits: give it back to the root window.

## Design principles

- A window is closed with `WM_DELETE_WINDOW` first; `XKillClient` is only a
  fallback.
- The state of a window is what the client and the standards say it is
  (`_NET_WM_STATE`, size hints, Motif hints). The frame follows the client,
  not the opposite.
- Behavior is configured, not themed: a small configuration file for key
  bindings and layout is fine, a theme engine is not.
- One look, scalable for high-DPI (`scale` in the configuration). Every size in code
  comes from the theme, never from a literal number.
- Robustness: a misbehaving client must not crash the window manager.
