# Writing plugins

A plugin adds something to the panel of tiny-shell, in Lua. This page is the
reference of what a plugin can do, and two small plugins to start from. How the
user installs and switches plugins is in [Plugins](../plugins.md).

The interface has a **level**, now 1 (`tiny.api`). A plugin says which level it
needs (`api` in `plugin.ini`); a shell that has an older level does not run it.

## A plugin is a directory

```
~/.local/share/tiny-shell/plugins/weather/
    plugin.ini      the name, the version and the settings
    plugin.lua      the code
    util.lua        anything else you load with require("util")
```

The name of the directory is the **id** of the plugin: lowercase letters, digits,
`-` and `_`. The user's directory is `$XDG_DATA_HOME/tiny-shell/plugins`, the system
ones are `tiny-shell/plugins` in the `$XDG_DATA_DIRS`; a plugin of the user
replaces a system one with the same id. The plugins that come with tiny-shell are
in `data/tiny-shell/plugins/`, the two examples below in `examples/plugins/`.

### plugin.ini

```ini
[plugin]
; shown in the list
name = Weather
description = The weather and the forecast
; compared as numbers: 1.10 is newer than 1.9
version = 1.2
author = A Name
url = https://example.org/weather
; the level of the interface that it needs
api = 1
; does it run before the user chooses? (default no)
enabled = no
; the settings, in the order of the dialog
settings = location;units

[setting.location]
type = text
label = Location
note = The name of a town, for example Prague
default = Prague

[setting.units]
type = choice
label = Units
choices = metric=Celsius;imperial=Fahrenheit
default = metric
```

A comment is a whole line that starts with `;` or `#`; a comment after a value
would be a part of the value (as in the ini file of the shell).

A setting is one of four types. The `default` is used until the user sets it;
a value that is not valid (not one of the choices, out of the range) is ignored
and the default is used.

| `type` | The user sees | Extra keys | In Lua |
| ------ | ------------- | ---------- | ------ |
| `bool` | Yes or No | | a boolean (`no` when no default) |
| `choice` | one of the values | `choices = value=Label;value2=Label2` | the text of the value |
| `text` | a line to type | | a string |
| `number` | a number to type | `min`, `max` | a number |

`tiny-settings` builds the dialog from this, so a plugin does not draw one; a plugin
that has no `settings` can only be switched on and off. A broken `plugin.ini`
does not stop the shell: the plugin is listed as broken with the reason.

## The module `tiny`

The module is the global `tiny` when `plugin.lua` runs. Every plugin has its own
Lua state, so globals are not shared between plugins.

| | |
| --- | --- |
| `tiny.api` | the level of the interface, 1 |
| `tiny.version` | the version of tiny-shell |
| `tiny.platform` | `"linux"`, `"freebsd"`, `"openbsd"`, `"netbsd"`, `"dragonfly"`, `"macos"` or `"unknown"` |
| `tiny.plugin` | a table with `id`, `name`, `version` and `dir` of this plugin |
| `tiny.setting(key)` | the value of a setting, typed as in the table above, or `nil` for a key that is not in `plugin.ini` |
| `tiny.config(section, key)` | what the configuration files of the shell say (`tiny.config("theme", "style")`), as text, or `nil` when the key is not set. Read only. |
| `tiny.theme()` | `"dark"` or `"light"`, the theme in use |
| `tiny.has_icon(name)` | `true` when the shell has the symbolic icon (see Icons below), so a plugin can use another one when a theme lacks it |
| `tiny.on_config(fn)` | `fn()` is called when the shell read its configuration again and something changed: a setting of this plugin, or any other key. `nil` removes it. See below. |
| `tiny.log(...)` | writes a line to the standard error of the shell, with the name of the plugin. `print` does the same. |
| `tiny.after(ms, fn)` | calls `fn()` once after `ms` milliseconds (10 at least); returns a timer with `:cancel()` |
| `tiny.every(ms, fn)` | calls `fn()` every `ms` milliseconds; returns a timer |
| `tiny.exec(argv, fn[, options])` | runs a program in the background, then calls `fn(result)`. See below. |
| `tiny.launch(argv)` | starts a program that is left running; returns `true`, or `false, error` |
| `tiny.read_file(path[, max])` | the content of a file (at most `max` bytes, 1 MiB by default), or `nil, error`. Works for the files of `/proc` and `/sys` too. |
| `tiny.list_dir(path)` | the names in a directory, sorted, or `nil, error` |
| `tiny.which(name)` | the path of a program that is in `PATH`, or `nil` |
| `tiny.json.decode(text)` | a Lua value from JSON, or `nil, error`; JSON `null` is `tiny.json.null` |
| `tiny.panel.label{...}`, `tiny.panel.button{...}` | a widget in the panel, see below |
| `require(name)` | loads `name.lua` from the directory of the plugin (`require("lib.util")` is `lib/util.lua`); only there |

`os.time`, `os.date`, `os.clock`, `os.getenv`, `os.difftime`, `string`, `table`,
`math`, `utf8` and `coroutine` are there. There is no `io`, no `package`, no
`dofile` and no `os.execute`: a plugin reads files and starts programs through
`tiny`. This keeps one way to do each thing; it is **not a sandbox**, because
`tiny.exec` runs any program, so a plugin is as trusted as any program of the user.

### Programs: `tiny.exec`

The shell never waits for a plugin. `tiny.exec` starts the program, returns at
once and calls the function later, from the loop of the shell:

```lua
tiny.exec({"sysctl", "-n", "hw.acpi.battery.life"}, function(result)
    if result.ok then
        tiny.log("charge", tonumber(result.stdout))
    end
end)
```

`argv` is a list of strings, with the program first (it is searched in `PATH`).
There is **no shell**: the arguments are not interpreted, so `"*"` and `"$HOME"`
mean what they say. The standard input of the program is empty (see `stdin`
below) and its standard error is discarded. `result` is a table:

| Field | |
| ----- | - |
| `ok` | `true` when the program ended with the status 0 |
| `status` | the exit status, `-1` when a signal ended it or it did not start |
| `stdout` | what it wrote (at most 1 MiB) |
| `truncated` | `true` when it wrote more |
| `timed_out` | `true` when it did not end in 30 seconds and was killed |
| `error` | why it did not start (`No such file or directory`), only then |

`options` is a table: `{timeout = 5000}` changes the time in milliseconds, and
`{stdin = "text"}` is what the program reads from its standard input (4096 bytes
at most). Give a password this way: on the command line every user of the system
sees it in `ps`. A program that can't be started is reported the same way, later (`ok = false` and
`error`), so the function is never called while the plugin is still in the
call that asked for it. The programs of a plugin that is stopped are killed.

### Widgets of the panel

```lua
local label = tiny.panel.label{text = "12:00", sample = "00:00 AM"}
local button = tiny.panel.button{
    icon = "battery-good", text = "85%", sample = "100%",
    on_click = function() ... end,
}
```

| | |
| --- | --- |
| `text` | what it shows |
| `sample` | the longest text that it will show. The panel gives the widget the width of the wider of `text` and `sample`, so the other widgets do not move when the text changes. A longer text is cut. |
| `icon` | the name of a symbolic icon that is drawn before the text in the color of the text, see Icons below. The widget keeps room for an icon when it is made with one, so changing it later does not move the others. |
| `icon_under` | an icon that is drawn first and dimmed, see Icons below |
| `on_click` | a function called on a click (a button only) |
| `w:set_text(text)` | changes the text |
| `w:text()` | the text now |
| `w:set_icon(name, under)` | changes the icons; `nil` for none. Only for a widget that was made with an `icon` (it may be `""`), because only then the widget has room for it; else it is an error |
| `w:set_visible(bool)` | hides and shows it; a hidden widget leaves its place empty, so create a widget only when there is something to show |
| `w:on_click(fn)` | replaces the function of the click (a button only) |
| `w:popup(window)` | opens a window under the widget, see Windows below |
| `w:popup_open()` | `true` while the window of this widget is open |
| `w:close_popup()` | closes it |
| `w:destroy()` | takes it out of the panel; also from its own click |

The widgets are added at the right of the panel, next to the clock, in the order
of the ids of the plugins and of the calls. They follow the theme. A plugin can
make widgets when it runs and later, from a timer or a program that ended. When
there is no panel the plugins are not started.

### Icons

The names are the symbolic icons of the shell: the files
`share/tiny-shell/icons/white/<name>-symbolic.svg` (`data/tiny-shell/icons` in
the repository), without `-symbolic.svg`. They follow the names of freedesktop.org
that GNOME uses, so `battery-good`, `weather-clear`, `network-wireless-signal-ok`,
`audio-volume-medium`, `bluetooth-active`, `cpu`, `memory` and the like are there.
An icon that does not exist is not drawn.

A level (the battery, the signal of the wifi, the volume) is a symbol that is
drawn whole and dimmed, and the part that is full over it. The icons are made
for it: `battery-full` under `battery-good`, `network-wireless-signal-excellent`
under `network-wireless-signal-ok`, `audio-volume-high` under `audio-volume-low`.
Give the full symbol as `icon_under` and the level as `icon`; for "no signal" give
only `icon_under`. The batteries that charge have their own icons with the bolt:
`battery-full-charging`, `battery-good-charging`, `battery-low-charging` and
`battery-empty-charging`; they need no `icon_under`.

### Windows

A click on a button can open a window with the details, like the weather of GNOME
Shell does. It opens under the widget, inside of the panel, and a click anywhere
else closes it, so does Escape. Every field is optional.

```lua
button:popup{
    icon = "weather-clear",          -- a big icon at the left of the header
    title = "Prague",
    subtitle = "Clear sky",          -- under the title, dimmed
    value = "9°C",                   -- big, at the right
    sections = {
        {heading = "Today", rows = {
            {label = "Wind", value = "16 km/h"},
            {icon = "weather-showers", label = "Friday", value = "3° / 8°"},
        }},
    },
}
```

A section has a `heading` and `rows`; a row has a `label` at the left, a `value`
at the right and an `icon` (the label of the rows of a section moves right when one
of them has an icon). There are at most 8 sections of 40 rows. Calling `popup`
again changes the window that is open. It also opens it, takes the keyboard focus
and puts the window over the others, so a plugin that wants to show a late answer
asks `w:popup_open()` first and calls `popup` only when it is `true`. A button
with text but without an icon has the text in the middle; a label has it at the left.

A row can have a `badge`, a small icon that is drawn before the `value` (a
lock for a network that has a password).

**Rows that do something.** A row with `on_click` is lit under the pointer and
when the arrow keys (or Tab) come to it; a click or Return calls the function. A
window is not closed by a click on a row: close it with `w:close_popup()` when
the plugin wants to. A row with `dim = true` is shown weaker and nothing
happens on it.

```lua
{icon = "network-wireless-signal-good", label = "Home", value = "WPA2",
 on_click = function() connect("Home") end}
```

**A field to type in.** A row with `entry = true` has a field instead of the
`value`; its `label` says what to type. The field has the keys as soon as the
window opens (the arrows leave it), `secret = true` shows dots, and Return calls
`on_submit(text)`. The text is wiped when the window closes. A window has one
field at most.

```lua
{label = "Password", entry = true, secret = true, id = "password",
 on_submit = function(text) connect("Home", text) end}
```

**Showing the window again.** When a plugin calls `popup` while the window is
open (an answer that came late), the window keeps what the user was doing: the
scrolling, the row that is lit and the text in the field. It knows a row by its
`id`, or by the `label` when there is no `id`; so give a row that can move in the
list the same `id` every time.

**A window that is too high** (a long list) is as high as the screen has room
for and is scrolled by the wheel, Page Up and Page Down, and by the arrows that
move the lit row; a thin bar at the right shows where it is.

### Settings that change

`tiny.setting()` always gives the value now. A plugin that wants to react when the
user changes something calls `tiny.on_config(fn)`:

- **With `tiny.on_config`** the plugin goes on running and `fn` is called when the
  shell read its configuration again and a setting of this plugin or any key of the
  shell changed (so also when the theme changed). It is also called for keys
  that are not yours; compare what you care about.
- **Without it** the plugin is stopped and started again when one of its own
  settings changes. Its widgets go to the end of the panel then.

When a plugin is switched on or off, or a new one is installed, every plugin is
stopped and started again, so the widgets stay in the order of the ids.

### Errors and limits

An error in `plugin.lua` stops that plugin (it is logged and listed as not
started); an error in a function that a timer, a program or a click calls is
logged, and the plugin goes on. A function that runs for more than two seconds
is stopped with an error: wait with `tiny.after`, `tiny.every` and `tiny.exec`
instead of a loop. Lua is not given a limit of memory.

## A plugin without a widget

`examples/plugins/hello-hidden/` shows nothing in the panel. It logs a greeting
every few seconds and has three settings, so it is a good place to try what the
settings do and what the shell tells the plugin.

```ini
[plugin]
name = Hello, hidden
description = Logs a greeting every few seconds. A plugin without a widget.
version = 1.0
author = Your Name
api = 1
enabled = no
settings = greeting;seconds;shout

[setting.greeting]
type = text
label = Greeting
note = What the plugin says
default = Hello

[setting.seconds]
type = number
label = Seconds between the greetings
min = 1
max = 3600
default = 10

[setting.shout]
type = bool
label = Say it loud
default = no
```

```lua
local timer

local function greet()
    local text = tiny.setting("greeting")
    if tiny.setting("shout") then
        text = text:upper() .. "!"
    end
    tiny.log(text, "(theme: " .. tiny.theme() .. ")")
end

local function start()
    if timer then
        timer:cancel()
    end
    timer = tiny.every(tiny.setting("seconds") * 1000, greet)
end

-- called when the user changes a setting (or the shell one of its own)
tiny.on_config(start)

tiny.log("started, version " .. tiny.plugin.version .. " of " .. tiny.plugin.name)
greet()
start()
```

## A plugin with a widget

`examples/plugins/hello-panel/` puts a button with an icon in the panel that counts the clicks and opens a window with the last one.

```lua
local clicks = 0
local button

local function shown()
    local text = tiny.setting("text")
    if clicks > 0 then
        if tiny.setting("format") == "time" then
            text = text .. " " .. os.date("%H:%M:%S")
        else
            text = text .. " " .. clicks
        end
    end
    return text
end

-- `sample` is the longest text of the button: the panel gives it that much
-- room, so the other widgets do not move when the text changes. `icon` is
-- a symbolic icon of the shell (the list is in data/tiny-shell/icons).
button = tiny.panel.button{
    text = shown(),
    sample = "A long greeting 00:00:00",
    icon = "emblem-shared",
    on_click = function()
        clicks = clicks + 1
        button:set_text(shown())
        -- the window under the button: a header and lines of details
        button:popup{
            icon = "emblem-shared",
            title = "Hello, panel",
            subtitle = "A plugin that counts clicks",
            value = tostring(clicks),
            sections = {
                {heading = "The last click", rows = {
                    {label = "Time", value = os.date("%H:%M:%S")},
                    {label = "Clicks so far", value = tostring(clicks)},
                }},
            },
        }
    end,
}

tiny.on_config(function()
    button:set_text(shown())
end)
```

A click on the button changes the text and opens a window under it. Its
`plugin.ini` has two settings, `text` and `format` (a choice of `clicks` and
`time`); see the file.

## Trying a plugin

```sh
mkdir -p ~/.local/share/tiny-shell/plugins
cp -r examples/plugins/hello-panel ~/.local/share/tiny-shell/plugins/
```

then open `tiny-settings`, choose Plugins, switch the plugin on, open its settings
(Right), change a value and save. The result shows at once. To see the messages of
`tiny.log` and the errors, start the shell from a terminal (`tools/run-xephyr.sh`
does it in a nested server) and look at its standard error. A plugin that
changed on disk is read again after `tiny-shell --reload` (a plugin that runs is
stopped and started when its set or its settings change; to load a changed
`plugin.lua`, switch the plugin off and on).

## Build and tests

The plugins are built when Lua 5.3 or newer is found by `pkg-config` (`lua5.4`,
`lua-5.4`, `lua54`, `lua5.3`...); `make LUA=no` leaves them out and `make LUA=yes`
insists on Lua. The code is in `lib/plugin/`: `plugin_info.*` (finding and reading
plugins, no Lua), `json.*`, `widgets.*`, `host.cc` (Lua) and `host_stub.cc` (the
build without Lua). The tests are in `tests/plugin/`; the host runs real Lua and
real programs against Xvfb. A new function of `tiny` needs a test there and an
entry on this page.
