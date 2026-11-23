-- The load of the computer in the panel: the processor, the memory, the swap
-- and the network (the download and the upload). Each of them is a button
-- with an icon and a number that a click opens in a window with the details.
-- The numbers are read where the system keeps them (linux.lua: /proc and /sys,
-- bsd.lua: sysctl, top, swapctl and netstat); model.lua turns the text into
-- numbers.
--
-- sample = {cpu, memory, swap, net}: any of them is nil when the system does
-- not tell (a computer without swap, without an adapter)

local model = require("model")

local backend
if tiny.platform == "linux" then
    backend = require("linux")
elseif tiny.platform == "freebsd" or tiny.platform == "openbsd" or tiny.platform == "netbsd" then
    backend = require("bsd")
else
    tiny.log("no reader for " .. tiny.platform)
    return
end

-- The buttons, in the order of the panel
local ICONS = {
    cpu = "cpu",
    memory = "memory",
    swap = "drive-harddisk", -- the swap is a piece of the disk
    download = "network-receive",
    upload = "network-transmit",
}
local ORDER = {"cpu", "memory", "swap", "download", "upload"}

local widgets = {} -- id -> button
local built = ""   -- which buttons exist, to know when to make them again

local interval = tiny.setting("interval")
local tick = 0         -- how many times the timer fired
local first            -- the first sample: the totals count from it
local last, last_tick  -- the sample before and when it was read
local busy = false     -- a reader is still running
local state = {}       -- the numbers that the buttons show

local function percent_mode()
    return tiny.setting("network_as") == "percent"
end

-- Which buttons the settings and the system want
local function wanted(sample)
    local ids = {}
    for _, id in ipairs(ORDER) do
        local on
        if id == "cpu" or id == "memory" then
            on = tiny.setting(id) and sample[id]
        elseif id == "swap" then
            on = tiny.setting("swap") and sample.swap
        else
            on = tiny.setting("network") and sample.net
        end
        if on then
            ids[#ids + 1] = id
        end
    end
    return ids
end

-- The text of a button
local function text_of(id)
    if id == "cpu" then
        return model.percent_text(state.cpu and state.cpu.busy)
    elseif id == "memory" or id == "swap" then
        return model.percent_text(model.share(state[id]))
    end
    local rate = state[id]
    if percent_mode() then
        return model.percent_text(model.link_share(rate, tiny.setting("link")))
    end
    return model.rate_text(rate)
end

local function row(label, value)
    return {label = label, value = value}
end

-- The window of a button
local function details(id)
    if id == "cpu" then
        local load = state.cpu
        local function share(name)
            return model.percent_text(load and load[name])
        end
        return {
            icon = "cpu", title = "Processor", subtitle = "All the processors together",
            value = model.percent_text(load and load.busy),
            sections = {{heading = "Time spent", rows = {
                row("Programs", share("user")), row("System", share("system")), row("Idle", share("idle")),
            }}},
        }
    elseif id == "memory" or id == "swap" then
        local part = state[id]
        local title = id == "memory" and "Memory" or "Swap"
        local subtitle = id == "memory" and "Used by the programs, without the cache" or "Memory that was moved to the disk"
        if not part then
            return {icon = ICONS[id], title = title}
        end
        return {
            icon = ICONS[id], title = title, subtitle = subtitle, value = model.percent_text(model.share(part)),
            sections = {{rows = {
                row("Used", model.bytes_text(part.used)),
                row("Free", model.bytes_text(part.total - part.used)),
                row("Total", model.bytes_text(part.total)),
            }}},
        }
    end
    local net = state.net
    local sections = {{heading = "Now", rows = {
        {icon = "network-receive", label = "Download", value = model.rate_long(state.download)},
        {icon = "network-transmit", label = "Upload", value = model.rate_long(state.upload)},
    }}}
    if net and first and first.net then
        sections[2] = {heading = "Since the plugin started", rows = {
            row("Received", model.bytes_text(net.rx - first.net.rx)),
            row("Sent", model.bytes_text(net.tx - first.net.tx)),
        }}
    end
    return {
        icon = "network-transmit-receive", title = "Network", subtitle = "All the adapters, without the loopback",
        sections = sections,
    }
end

local function open(id)
    -- the two buttons of the network open the same window
    local key = (id == "download" or id == "upload") and "network" or id
    local widget = widgets[id]
    if widget then
        widget:popup(details(key))
    end
end

-- Puts the numbers on the buttons, and in the window that is open
local function show()
    for id, widget in pairs(widgets) do
        widget:set_text(text_of(id))
        if widget:popup_open() then
            open(id)
        end
    end
end

-- Makes the buttons again when the set of them changed
local function build(sample)
    local ids = wanted(sample)
    local key = table.concat(ids, ",") .. (percent_mode() and "%" or "")
    if key == built then
        return
    end
    built = key
    for _, widget in pairs(widgets) do
        widget:destroy()
    end
    widgets = {}
    -- the first widget of the panel is the one next to the clock and the next
    -- ones are to its left, so the last one is made first
    for i = #ids, 1, -1 do
        local id = ids[i]
        local network = id == "download" or id == "upload"
        widgets[id] = tiny.panel.button{
            icon = ICONS[id], text = "--", sample = network and not percent_mode() and "999M" or "100%",
            on_click = function()
                if widgets[id]:popup_open() then
                    widgets[id]:close_popup()
                else
                    open(id)
                end
            end,
        }
    end
end

local function absorb(sample)
    local seconds = (tick - (last_tick or tick)) * interval
    if last then
        state.cpu = model.cpu_load(last.cpu, sample.cpu) or state.cpu
        state.download = sample.net and last.net and model.rate(last.net.rx, sample.net.rx, seconds) or state.download
        state.upload = sample.net and last.net and model.rate(last.net.tx, sample.net.tx, seconds) or state.upload
    end
    state.memory, state.swap, state.net = sample.memory, sample.swap, sample.net
    last, last_tick = sample, tick
    first = first or sample
end

local function update()
    if busy then
        return
    end
    busy = true
    tick = tick + 1
    backend.read(function(sample)
        busy = false
        if not sample then
            return
        end
        absorb(sample)
        build(sample)
        show()
    end)
end

local timer
local function start()
    if timer then
        timer:cancel()
    end
    interval = tiny.setting("interval")
    -- the speeds are counted over the time between two samples
    last, last_tick = nil, nil
    timer = tiny.every(interval * 1000, update)
end

-- A setting changed: the buttons that are shown, the pace, the way the
-- network is shown. The shell also calls this for keys that are not ours.
tiny.on_config(function()
    local previous = interval
    if tiny.setting("interval") ~= previous then
        start()
    end
    if last then
        build(last)
        show()
    end
end)

start()
update()
