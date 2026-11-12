# The session

How a session starts, which programs start with it, and how it ends.

## Starting

A display manager or `startx` runs the script `tiny-shell-session`, which
prepares the environment and starts the shell, see
[Starting a session](install.md#starting-a-session). Then the shell starts the
programs of the autostart.

### The environment file

`~/.config/tiny-shell/environment` (in `$XDG_CONFIG_HOME` when that is set) does
not exist until you make it. The session script reads it as a shell script before
it starts the window manager, so it is the place for what every program of the
session should see, or for what must run first:

```sh
export GDK_SCALE=2
export EDITOR=vim
setxkbmap cz
xset r rate 250 40
```

## Autostart

The programs that start with the session are the `.desktop` files in the
directories `autostart` of the configuration, as the
[XDG Autostart specification](https://specifications.freedesktop.org/autostart-spec/latest/)
says. GNOME, KDE and Xfce use the same files, so what a package puts in
`/etc/xdg/autostart` (a network applet, a volume control) starts here too.

| Directory | What it is |
| --------- | ---------- |
| `~/.config/autostart` | your programs; a file here wins over the same name in the system directory |
| `/etc/xdg/autostart` | the programs that the packages start with every session |

To start a program, make a file there, for example
`~/.config/autostart/clipboard.desktop`:

```ini
[Desktop Entry]
Type=Application
Name=Clipboard manager
Exec=clipit
```

To turn off a program of the system, copy its file to `~/.config/autostart` and
add `Hidden=true`. A file is also skipped when:

- it has `X-GNOME-Autostart-enabled=false` (how the settings of GNOME switch a
  program off),
- `OnlyShowIn` does not name the desktop `tiny-shell` or `NotShowIn` does (the
  session script sets the desktop in `XDG_CURRENT_DESKTOP`; a display manager
  sets it from its session file),
- the program named by `TryExec` is not installed.

The programs start once the panel and its [tray](configuration.md#the-system-tray)
are there, so the programs with a tray icon find it; they start in the order of
the file names, and a program that starts again in the same session (the shell
crashed, `tiny-shell --replace`) is not started twice. `X-GNOME-Autostart-Delay`
and `AutostartCondition` are not used. `[session] autostart = no` turns the
whole thing off.

## Ending the session

**Logout**, **Restart** and **Shutdown** of the main menu do not kill the
programs; they ask them to close, so a program can ask whether to save:

1. A dialog asks you to confirm. Return or the button goes on, Escape or Cancel
   stops it, and when you do not answer the session goes on by itself after 60
   seconds.
2. Every window gets the same request as the close button of its title bar. The
   dialog shows which applications are still open ("Waiting for Text Editor to
   close") and waits for them, as long as it takes: answer the question of the
   program and it closes. **Cancel** stops the logout (what is closed stays
   closed), **Log Out Anyway** does not wait for what is left.
3. When nothing is left, a logout ends the shell and with it the session; Restart
   and Shutdown run `[commands] reboot` and `poweroff`. If the command fails, the
   dialog says so.

Windows that cannot take the request, the bars of other programs and tooltips are
not waited for. A program without a window ends with the X server.

`[session] confirm = no` leaves out the confirmation, the programs are still asked
to close. `pkill tiny-shell` ends the shell at once, and the programs lose their
connection to the display.

## Saving the session

tiny-shell does not remember which programs ran or where their windows were: the
next login starts the autostart programs only. The X Session Management protocol
(XSMP) does that for the programs that support it. Openbox, Xfce and KDE have it,
GNOME removed it in version 49, and macOS has its own "Reopen windows when
logging back in". Many current programs ignore XSMP (a browser restores its
windows itself), so it would restore only a part of the session. It is not
planned, but not ruled out either (`?` in the [comparison](comparison.md)).
