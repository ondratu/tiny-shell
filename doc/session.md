# The session

What happens between the login and the logout: how a session starts, which
programs start with it, and (later) how it ends.

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
