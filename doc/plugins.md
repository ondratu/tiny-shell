# Plugins

Plugins add small things to the panel: the charge of the battery, the weather,
a monitor of the processor. A plugin is a few lines of [Lua](https://www.lua.org/)
that tiny-shell runs for you; it can show a text or a button in the panel, ask a
program of the system for what it needs, and have settings of its own.

Three plugins come with tiny-shell:

| Plugin | What it does | Switched on |
| ------ | ------------ | ----------- |
| **Battery** | A battery with the percentage next to it. The icon follows the charge (full, good, low, empty) and has a bolt while the battery charges. A click shows the state in words. Linux, FreeBSD and OpenBSD; a computer without a battery shows nothing. | yes |
| **Network** | The connection of the computer: a cable, or the Wi-Fi with the strength of its signal. A click lists every adapter with its addresses and speed and the Wi-Fi networks in range, and lets you connect, disconnect and turn an adapter off. Below is what each system can do. | yes |
| **Weather** | The symbol of the weather and the temperature, from [open-meteo.com](https://open-meteo.com/) (free, no account; needs `curl`). A click opens a window like the one of GNOME: the town, the weather now, the feeling, the humidity, the wind, the pressure and the forecast for the next five days. Nothing changes by itself. | no, it needs a town first |

How to write your own is in [Writing plugins](developers/plugins.md).

## The network

The icon in the panel shows the connection that the computer uses: a cable, or
the Wi-Fi as a symbol of waves that fill up with the strength of the signal (the
whole symbol is drawn dimmed and the strength over it). Without a connection it
shows a Wi-Fi that is off, or a cable that is out. A computer without a network
card shows nothing. In the settings of the plugin the name of the Wi-Fi can be
shown next to the icon.

A click opens a window with **every adapter**, one section for each, like the
main menu:

- **Ethernet**: connected or not, the speed, the name of the connection, the
  IPv4 and IPv6 addresses (the link-local ones only when there is no other), and
  a row to turn the cable off and on.
- **Wi-Fi**: the network that it is connected to, with the signal, the band
  (2.4, 5 or 6 GHz), the speed, the security and the addresses; **Disconnect**
  and a row to turn the Wi-Fi off and on. Below, the **networks in range**, the
  strongest first, with the strength of the signal, the band, a lock when the
  network has a password and **Saved** for the ones that the computer knows.
  A click connects; a network that needs a password asks for it in the window.

The window is scrolled when it is higher than the screen (the wheel, Page Up and
Page Down, the arrows) and follows the changes of the network while it is open.
The networks are searched for when the window opens and every 15 seconds while
it is open; nothing is searched for while it is closed, as that costs power.

What the plugin can do depends on the system:

| System | Looks | Changes |
| ------ | ----- | ------- |
| Linux with NetworkManager (`nmcli`) | everything above | connect, disconnect, turn on and off, a new network with a password |
| Linux without it | state, addresses, speed, and the signal (`ip` is needed, `iw` gives the name of the Wi-Fi) | no |
| FreeBSD, OpenBSD, NetBSD, DragonFly | state, addresses, speed, the name of the Wi-Fi and its signal where `ifconfig` tells it | no, use the tools of the system |

A row that you may not use is shown dimmed with the reason under it: the
adapter is not managed by NetworkManager (a static address in the configuration
of the system), the policy of the system does not allow the change for your user,
or the system is not one that the plugin can change yet. The addresses are
always shown.

The password goes to `nmcli` on its standard input, not in its arguments, so it
is not seen in the list of processes, and it is wiped from the window when the
window closes. NetworkManager saves the network the way it saves the networks
that other programs add.

Not there yet: networks with a hidden name, the login of an enterprise network
(802.1X; a saved one can be connected), VPN, and settings of an address. The
plugin is made of a part that shows (`view.lua`) and one backend for each kind of
system (`nm.lua`, `sys.lua`, `bsd.lua`), so another system is another file, see
[Writing plugins](developers/plugins.md#a-bigger-plugin-the-network).

The settings are **Show the name of the Wi-Fi** (no by default) and **Seconds
between the checks** (10; the window checks every 3 seconds while it is open).

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
