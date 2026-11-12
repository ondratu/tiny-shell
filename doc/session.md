# The session

How a session starts, which programs start with it, and how it ends.

## Starting

A display manager or `startx` runs the script `tiny-shell-session`, which
prepares the environment and starts the shell, see
[Starting a session](install.md#starting-a-session). Then the shell starts the
programs of the autostart.

### The environment file

Variables for everything you run (`EDITOR`, `PATH`) belong to `~/.profile`: GDM
and SDDM read it before the session starts, and with `startx` you have logged in
already. Some display managers do not read it (LightDM, depending on the
version), so check yours.

`~/.config/tiny-shell/environment` (in `$XDG_CONFIG_HOME` when that is set) is for
what concerns the graphical session only. It does not exist until you make it,
and the session script reads it as a shell script after the profile and before
it starts the shell:

```sh
export GDK_SCALE=2
setxkbmap cz
xset r rate 250 40
```

Commands for the X server (`setxkbmap`, `xset`) would fail in a login on a
text console, which is why they do not belong to the profile. Openbox has the
same file in `~/.config/openbox/environment`.

## Autostart

The programs that start with the session are the `.desktop` files in the
directories `autostart` of the configuration, as the
[XDG Autostart specification](https://specifications.freedesktop.org/autostart-spec/latest/)
says, so what a package puts in `/etc/xdg/autostart` (a network applet, a
volume control) starts here too.

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

### Which programs start

A file is skipped when it has `Hidden=true`, or when:

- it has `X-GNOME-Autostart-enabled=false`,
- `OnlyShowIn` does not name the desktop `tiny-shell` or `NotShowIn` does (the
  session script sets the desktop in `XDG_CURRENT_DESKTOP`; a display manager
  sets it from its session file),
- it has an `AutostartCondition`, which tiny-shell cannot check,
- the program named by `TryExec` is not installed.

The programs start once the panel and its [tray](configuration.md#the-system-tray)
are there, so the programs with a tray icon find it; they start in the order of
the file names, and a program that starts again in the same session (the shell
crashed, `tiny-shell --replace`) is not started twice. `[session] autostart = no`
turns the whole thing off (the setting Autostart in the section Session of
`tiny-settings`).

### Switching programs on and off

The field Programs of the section Session in `tiny-settings` lists every
program of both directories. A click or Space switches a program, Done keeps the
choice and Save writes it. The list marks where a program comes from:

- **System**: a file in `/etc/xdg/autostart`. It is never changed or removed;
  switching it off makes a file of the same name in `~/.config/autostart` that
  holds `Hidden=true` and nothing else, and switching it on removes that file.
- **User**: a file in `~/.config/autostart`. Switching it off adds `Hidden=true`
  to the file and switching it on takes the line away, so the file stays.

A program that does not start for another reason (see
[Which programs start](#which-programs-start)) is dimmed with the reason and
cannot be switched.

Without `tiny-settings` you do the same by hand: copy the file of the system to
`~/.config/autostart` and add `Hidden=true`, or delete your own file.

## Ending the session

**Logout**, **Restart** and **Shutdown** of the main menu do not kill the
programs; they ask them to close, so a program can ask whether to save:

1. With `[session] confirm = yes` a dialog asks you to confirm first (Return or
   the button goes on, Escape or Cancel stops it, and when you do not answer the
   session goes on by itself after 60 seconds). By default there is none.
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

`[session] confirm = yes` (the setting Confirm in the section Session of
`tiny-settings`) turns on the dialog with the countdown; the programs are asked
to close either way.

`pkill tiny-shell` ends the shell at once, and the programs lose their
connection to the display.

## Saving the session

tiny-shell does not remember which programs ran or where their windows were: the
next login starts the autostart programs only. The X Session Management protocol
(XSMP) does that for the programs that support it, but many current programs
ignore it, so it would restore only a part of the session. It is not planned,
but not ruled out either (`?` in the [comparison](comparison.md)).
