-- The network in the panel: the icon of the connection (a cable, or the Wi-Fi
-- with the strength of the signal) and a window with every adapter: the state,
-- the addresses, the speed, and for a Wi-Fi the networks in range, with the
-- way to connect to one and to disconnect.
--
-- The window is made from a list of adapters that a backend gives (model.lua
-- says what is in it); the backends are
--   nm.lua   Linux with NetworkManager: looks and changes (the program nmcli)
--   sys.lua  Linux without it: looks (/sys, /proc, ip, iw)
--   bsd.lua  FreeBSD, OpenBSD, NetBSD, DragonFly: looks (ifconfig)
-- view.lua makes the icon and the window of them. Nothing is linked: the
-- plugin only runs programs, so the shell does not need a library for it.

local model = require("model")
local view = require("view")

local backend
if tiny.platform == "linux" then
    backend = tiny.which("nmcli") and require("nm") or require("sys")
elseif tiny.platform == "freebsd" or tiny.platform == "openbsd"
    or tiny.platform == "netbsd" or tiny.platform == "dragonfly" then
    backend = require("bsd")
else
    tiny.log("no network reader for " .. tiny.platform)
    return
end

local LOOK_EVERY = 3      -- seconds between the looks while the window is open
local SCAN_EVERY = 15     -- seconds between the searches for Wi-Fi networks
local STALE = 40          -- seconds after which a look that did not end is forgotten
local SEARCH_SHOWN = 6    -- seconds that the window says "Searching"

local lock_icon = tiny.has_icon("network-wireless-encrypted") and "network-wireless-encrypted"
    or "system-lock-screen"
local show_name = tiny.setting("ssid")

local widget
local adapters = {}
local reading_since     -- when a look began, nil when none runs
local window_timer
local last_scan = 0
local ui = {on = {}}

local function render_panel()
    local panel = view.panel(adapters, {ssid = show_name})
    if not panel.visible then
        if widget then
            widget:close_popup()
            widget:set_visible(false)
        end
        return
    end
    if not widget then
        widget = tiny.panel.button{
            text = panel.text, sample = show_name and "mmmmmmmmmmmmmmmm" or "",
            icon = panel.icon or "", icon_under = panel.under,
            on_click = function() ui.open() end,
        }
        return
    end
    widget:set_visible(true)
    widget:set_icon(panel.icon, panel.under)
    widget:set_text(panel.text)
end

local function show_window()
    if widget then
        widget:popup(view.popup(adapters, ui, lock_icon))
    end
end

-- A late answer changes the window only when it is still open
local function refresh_window()
    if widget and widget:popup_open() then
        show_window()
    end
end

local function look(done)
    if reading_since and os.time() - reading_since < STALE then
        return
    end
    reading_since = os.time()
    backend.read(function(list)
        reading_since = nil
        adapters = list
        render_panel()
        refresh_window()
        if done then
            done()
        end
    end)
end

-- Asks every Wi-Fi to search again; what it finds comes with the next looks
local function search()
    if not backend.scan then
        return
    end
    last_scan = os.time()
    ui.scanning = true
    for _, adapter in ipairs(adapters) do
        if adapter.can_scan and adapter.radio ~= false then
            backend.scan(adapter, function() end)
        end
    end
    tiny.after(SEARCH_SHOWN * 1000, function()
        ui.scanning = false
        refresh_window()
    end)
end

-- While the window is open the look is often and a search is now and then
local function keep_looking()
    if window_timer then
        return
    end
    window_timer = tiny.every(LOOK_EVERY * 1000, function()
        if not (widget and widget:popup_open()) then
            window_timer:cancel()
            window_timer = nil
            return
        end
        look()
        if os.time() - last_scan >= SCAN_EVERY then
            search()
        end
    end)
end

function ui.open()
    ui.message, ui.asking = nil, nil
    show_window()
    keep_looking()
    look()
    search()
end

-- What was done to an adapter is told in the window when it did not work
local function after_change(problem)
    ui.message = problem
    look()
    tiny.after(1500, look) -- the system needs a moment to change
    refresh_window()
end

local function enabled(adapter)
    if adapter.kind == "wifi" then
        return adapter.radio ~= false
    end
    return adapter.state == "connected" or adapter.state == "connecting"
end

function ui.on.toggle(adapter)
    ui.message = enabled(adapter) and "Turning off…" or "Turning on…"
    show_window()
    backend.toggle(adapter, not enabled(adapter), after_change)
end

function ui.on.disconnect(adapter)
    ui.message = "Disconnecting…"
    show_window()
    backend.disconnect(adapter, after_change)
end

local function connect(adapter, network, password)
    ui.asking = nil
    ui.message = "Connecting to " .. network.ssid .. "…"
    show_window()
    backend.connect(adapter, network, password, after_change)
end

function ui.on.connect(adapter, network)
    if network.known or not model.is_secured(network.security) then
        return connect(adapter, network, nil)
    end
    ui.message = nil
    ui.asking = {ssid = network.ssid, adapter = adapter, network = network}
    show_window()
end

function ui.on.submit(text)
    local asking = ui.asking
    if asking then
        connect(asking.adapter, asking.network, text)
    end
end

function ui.on.cancel()
    ui.asking = nil
    show_window()
end

look()
tiny.every(tiny.setting("interval") * 1000, function() look() end)
