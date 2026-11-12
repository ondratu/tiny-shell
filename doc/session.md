# The session

What happens between the login and the logout: how a session starts, which
programs start with it, and how it ends so that nobody loses their work.

## How a session starts

A display manager or `startx` runs the script `tiny-shell-session`, which
prepares the environment and starts the window manager; see
[how to start it](install.md#starting-a-session). The window manager then
starts the programs of the autostart directories.

## Autostart

The programs that start with the session are the files `*.desktop` in the
directories `autostart` of the configuration, as the
[XDG Autostart specification](https://specifications.freedesktop.org/autostart-spec/latest/)
says. The same files are used by GNOME, KDE and Xfce, so what a package puts in
`/etc/xdg/autostart` (a network applet, a volume control, a clipboard manager)
starts here too.

| Directory | What it is |
| --------- | ---------- |
| `~/.config/autostart` (`$XDG_CONFIG_HOME/autostart`) | the programs of the user; wins over the same file name of the system |
| `/etc/xdg/autostart` (`$XDG_CONFIG_DIRS`) | the programs that the packages of the system start with every session |

To start a program with the session, make a file there, for example
`~/.config/autostart/clipboard.desktop`:

```ini
[Desktop Entry]
Type=Application
Name=Clipboard manager
Exec=clipit
```

To turn off what the system starts, copy its file into `~/.config/autostart`
and add `Hidden=true` to it (the file of the user hides the one of the system).
`X-GNOME-Autostart-enabled=false` does the same, as the settings of GNOME do it.

The shell starts the programs once the panel and its
[system tray](configuration.md#the-system-tray) are there, so the programs that put
an icon in the tray find it. Not every file is started:

- `Hidden=true` and `X-GNOME-Autostart-enabled=false` turn it off,
- `OnlyShowIn` and `NotShowIn` name the desktops (`XDG_CURRENT_DESKTOP`) that run
  it; the desktop of the session is `tiny-shell`, so a file for `GNOME` only is
  not started, and a file for `tiny-shell` is (a display manager sets the value
  from the file of the session, and the shell sets it when nobody did),
- `TryExec` names a program that must be installed,
- `Terminal=true` runs it in the terminal of `[commands] terminal`.

The files are started in the order of their names, without waiting for each other.
`X-GNOME-Autostart-Delay` and `AutostartCondition` are not used.

A session starts them once: a window manager that starts again in the same
session (it crashed, or you run `tiny-shell --replace`) does not start them a
second time. Logging out and in is a new session. `[session] autostart = no` in
the [configuration file](configuration.md) switches the whole thing off.

## Ending the session

**Logout**, **Restart** and **Shutdown** of the main menu (and the actions
`logout`, `restart` and `shutdown`) end the session the way GNOME and macOS do.
They do not kill the programs; they ask them to close, so that a program can ask
whether to save its work:

1. **A question.** A dialog says "You will be logged out automatically in 60
   seconds." Return (or the button) goes on, Escape (or Cancel) stops, and when
   nobody answers the session goes on by itself after a minute. This is the
   countdown of GNOME and macOS.
2. **The windows are asked to close.** Every window gets `WM_DELETE_WINDOW`, the
   same request as the close button of its title bar, so a text editor asks
   "Save changes?". The dialog now says which applications are still open
   ("Waiting for Text Editor to close.") and the shell waits for them without a
   time limit, because only you know if the editor is just slow or waits for
   your answer. A question of the program is an ordinary window: answer it, and
   the program closes. Cancel in the dialog stops the logout (what was closed stays
   closed). **Log Out Anyway** does not wait for what is left.
3. **The end.** When no application is left, a logout ends the shell, and with it
   the session (the display manager shows the login screen, `startx` returns to
   the terminal). Restart and Shutdown run `[commands] reboot` and `poweroff`.
   When the command fails (a missing program, no right to run it), the dialog
   says so, because all the programs are closed by then.

Only the windows that can take the request count: a window without
`WM_DELETE_WINDOW` is closed with the session, and the bars of other programs
(`_NET_WM_WINDOW_TYPE_DOCK`), a desktop and tooltips are not waited for. A
program that keeps running without a window (a background service) is ended by
the end of the X server.

`[session] confirm = no` leaves out the question, and the programs are asked
to close at once (the shell still waits for them). To skip the whole thing, send
`SIGTERM` to `tiny-shell` (`pkill tiny-shell`): it ends at once and the programs
lose the connection to the display.

### What a session does not save

tiny-shell does not remember which programs run or where their windows were: the
next login starts with the autostart programs only. A window manager can do it
with the X Session Management protocol (XSMP), which almost no current program
supports. The [comparison](comparison.md) says what other window
managers do; it is not planned, but it is not ruled out.
