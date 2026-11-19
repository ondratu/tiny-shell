# Plugins

Plugins add small things to the panel: the charge of the battery, the weather,
a monitor of the processor. A plugin is a few lines of [Lua](https://www.lua.org/)
that tiny-shell runs for you; it can show a text or a button in the panel, ask a
program of the system for what it needs, and have settings of its own.

Two plugins come with tiny-shell:

| Plugin | What it does | Switched on |
| ------ | ------------ | ----------- |
| **Battery** | A battery with the percentage next to it. The icon follows the charge (full, good, low, empty) and has a bolt while the battery charges. A click shows the state in words. Linux, FreeBSD and OpenBSD; a computer without a battery shows nothing. | yes |
| **Weather** | The symbol of the weather and the temperature, from [open-meteo.com](https://open-meteo.com/) (free, no account; needs `curl`). A click opens a window like the one of GNOME: the town, the weather now, the feeling, the humidity, the wind, the pressure and the forecast for the next five days. Nothing changes by itself. | no, it needs a town first |

How to write your own is in [Writing plugins](developers/plugins.md).

## Switching plugins on and off

Open `tiny-settings` and choose **Plugins**. The list shows every plugin that is
installed, with a box that is filled when the plugin runs and where the plugin
comes from:

- **System**: it came with tiny-shell (or with a package of your system).
- **User**: you copied it to your own directory (see below).
- **User, replaces 1.0**: you have a plugin with the same name as a system one;
  yours is used and the list says which version it replaces.
- **Broken**: its `plugin.ini` or `plugin.lua` is wrong, the line under the list
  tells what. A broken plugin can't be switched on.

| Key or click | What it does |
| ------------ | ------------ |
| Space, a click | Switches the plugin on or off |
| Right, a click at the end of the row | Opens the settings of the plugin, when it has some |
| Delete | Marks a plugin of yours to be removed when you save (the directory is deleted) |
| Return, Done | Keeps what you did in the list |
| Escape, Cancel | Brings it back |

Press **Save** in `tiny-settings` and the shell starts and stops the plugins at
once, without a restart.

## The settings of a plugin

A plugin that has settings shows them as a list: a choice goes through its
values with a click or the arrows, a text is typed, a number is typed and must be
in the range that the plugin says. The Battery has whether to show the
percentage and how often to look; the Weather has the town, the units (Celsius
or Fahrenheit) and how often to update.

The values are saved in your `tiny-shell.ini` in the section of the plugin:

```ini
[plugin.weather]
enabled = yes
location = Prague
units = metric
interval = 30
```

`enabled` is the switch; the other keys are the settings of that plugin. You can
edit the file by hand as well; the shell reads it again after `SIGHUP` or
`tiny-shell --reload`.

## Where plugins are

A plugin is a directory with the name of the plugin, as for the extensions of GNOME
Shell. The shell looks in

1. `$XDG_DATA_HOME/tiny-shell/plugins/` (`~/.local/share/tiny-shell/plugins/`): your plugins,
2. `tiny-shell/plugins/` in each of the `$XDG_DATA_DIRS` (`/usr/local/share` and
   `/usr/share` by default): the plugins of the system. `make install` puts the
   ones of tiny-shell into `$PREFIX/share/tiny-shell/plugins/`.

To install a plugin that you have downloaded, copy its directory to the first
place and switch it on in the list:

```sh
mkdir -p ~/.local/share/tiny-shell/plugins
cp -r some-plugin ~/.local/share/tiny-shell/plugins/
tiny-shell --reload
```

To remove it, delete it in the list (Delete, then Save) or delete the directory.
Plugins of the system can only be switched off, not deleted from there.

To try the plugins of a source tree that is not installed, run the shell with
`XDG_DATA_DIRS=$PWD/data:/usr/share build/tiny-shell` (`tools/run-xephyr.sh`
does it).

## A plugin is a program

A plugin can run any program and read any file that you can. Install only the
plugins that you trust, as you would any other program. A plugin cannot freeze
the shell: a program that it starts runs in the background, and a function that
runs for more than two seconds is stopped. An error in a plugin stops only that
plugin; the message is on the standard error of tiny-shell (the file of your
session, `~/.xsession-errors` in many systems).

## Without Lua

The plugins need Lua 5.3 or newer (`liblua5.4-dev`, `lua-devel`, `lua5.4-dev`
and `lua54` on the BSD systems). The build uses it when it finds it; `make LUA=no`
builds a shell without plugins that is a little smaller, and `make LUA=yes` fails
when Lua is not found. A shell without Lua still lists the plugins in
`tiny-settings` and says that they do not run. See [Installation](install.md).
