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
name = Weather                  ; shown in the list
description = The weather and the forecast
version = 1.2                   ; compared as numbers: 1.10 is newer than 1.9
author = A Name
url = https://example.org/weather
api = 1                         ; the level of the interface that it needs
enabled = no                    ; does it run before the user chooses? (default no)
settings = location;units       ; the settings, in the order of the dialog

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
mean what they say. The standard input of the program is empty and its standard
error is discarded. `result` is a table:

| Field | |
| ----- | - |
| `ok` | `true` when the program ended with the status 0 |
| `status` | the exit status, `-1` when a signal ended it or it did not start |
| `stdout` | what it wrote (at most 1 MiB) |
| `truncated` | `true` when it wrote more |
| `timed_out` | `true` when it did not end in 30 seconds and was killed |
| `error` | why it did not start (`No such file or directory`), only then |

`options` is a table: `{timeout = 5000}` changes the time in milliseconds. A
program that can't be started is reported the same way, later (`ok = false` and
`error`), so the function is never called while the plugin is still in the
call that asked for it. The programs of a plugin that is stopped are killed.

### Widgets of the panel

```lua
local label = tiny.panel.label{text = "12:00", sample = "00:00 AM"}
local button = tiny.panel.button{text = "Go", on_click = function() ... end}
```

| | |
| --- | --- |
| `text` | what it shows |
| `sample` | the longest text that it will show. The panel gives the widget the width of the wider of `text` and `sample`, so the other widgets do not move when the text changes. A longer text is cut. |
| `on_click` | a function called on a click (a button only) |
| `w:set_text(text)` | changes the text |
| `w:text()` | the text now |
| `w:set_visible(bool)` | hides and shows it; a hidden widget leaves its place empty, so create a widget only when there is something to show |
| `w:on_click(fn)` | replaces the function of the click (a button only) |
| `w:destroy()` | takes it out of the panel; also from its own click |

The widgets are added at the right of the panel, next to the clock, in the order
of the ids of the plugins and of the calls. They follow the theme. A plugin can
make widgets when it runs and later, from a timer or a program that ended. When
there is no panel the plugins are not started.

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

`examples/plugins/hello-panel/` puts a button in the panel that counts the clicks.

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

button = tiny.panel.button{
    text = shown(),
    sample = "A long greeting 00:00:00",
    on_click = function()
        clicks = clicks + 1
        button:set_text(shown())
    end,
}

tiny.on_config(function()
    button:set_text(shown())
end)
```

Its `plugin.ini` has two settings, `text` and `format` (a choice of `clicks` and
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
