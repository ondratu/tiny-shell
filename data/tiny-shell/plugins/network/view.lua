-- What the plugin shows: the icon of the panel and the window of a click, made
-- from the adapters (see model.lua). Nothing here talks to the system, so the
-- tests look at the results.

local model = require("model")
local util = require("util")

local view = {}

local MAX_SECTIONS = 8   -- the limit of a window
local MAX_NETWORKS = 30  -- the networks in range that the window lists
local NAME_LIMIT = 16    -- characters of the name of a Wi-Fi in the panel

local function shorten(text, limit)
    if utf8.len(text) and utf8.len(text) > limit then
        return text:sub(1, utf8.offset(text, limit + 1) - 1) .. "…"
    end
    return text
end

-- The icon, the dimmed icon under it and the text of the panel
function view.panel(adapters, options)
    if #adapters == 0 then
        return {visible = false}
    end
    local main = model.primary(adapters)
    local show_name = options and options.ssid
    if main and main.kind == "wifi" then
        local wifi = main.wifi or {}
        local text = show_name and wifi.ssid and shorten(wifi.ssid, NAME_LIMIT) or ""
        if wifi.signal == nil then -- connected, the system does not tell the signal
            return {visible = true, icon = model.FULL, text = text}
        end
        return {visible = true, icon = model.level_icon(wifi.signal), under = model.FULL, text = text}
    elseif main then
        return {visible = true, icon = "network-wired", text = ""}
    end
    local has_wifi, connecting = false, false
    for _, adapter in ipairs(adapters) do
        has_wifi = has_wifi or adapter.kind == "wifi"
        connecting = connecting or adapter.state == "connecting"
    end
    if connecting and has_wifi then
        return {visible = true, under = model.FULL, text = ""}
    end
    return {visible = true, icon = has_wifi and "network-wireless-offline" or "network-wired-disconnected", text = ""}
end

local function summary(adapters)
    local main = model.primary(adapters)
    if not main then
        return "Not connected"
    elseif main.kind == "wifi" then
        return "Connected to " .. ((main.wifi and main.wifi.ssid) or "Wi-Fi")
    end
    return "Connected by cable"
end

local function address_rows(rows, adapter)
    local picked = {
        {"IPv4", util.pick_addresses(adapter.ipv4, 2)},
        {"IPv6", util.pick_addresses(adapter.ipv6, 3)},
    }
    for _, entry in ipairs(picked) do
        for i, address in ipairs(entry[2]) do
            rows[#rows + 1] = {label = i == 1 and entry[1] or "", value = address}
        end
    end
end

local function switch_row(adapter, text, ui)
    return {
        label = text, id = "toggle:" .. adapter.name,
        on_click = function() ui.on.toggle(adapter) end,
    }
end

local function reason_rows(rows, adapter)
    if adapter.reason then
        rows[#rows + 1] = {label = adapter.reason, dim = true}
    end
end

local function ethernet_section(adapter, ui)
    local rows = {}
    local words = {
        connected = "Connected", connecting = "Connecting…", disabled = "Turned off",
        unavailable = "Cable unplugged",
    }
    local word = words[adapter.state]
    if adapter.state == "disconnected" then
        word = adapter.carrier and "Disconnected" or "Cable unplugged"
    end
    rows[#rows + 1] = {
        icon = adapter.state == "connected" and "network-wired" or "network-wired-disconnected",
        label = word or "Not connected", value = model.speed_text(adapter.speed) or "",
    }
    if adapter.connection and adapter.state == "connected" then
        rows[#rows + 1] = {label = "Connection", value = adapter.connection}
    end
    if adapter.state == "connected" then
        address_rows(rows, adapter)
    end
    if adapter.can_toggle then
        local on = adapter.state == "connected" or adapter.state == "connecting"
        if on or adapter.carrier or adapter.state == "disconnected" then
            rows[#rows + 1] = switch_row(adapter, on and "Turn off" or "Turn on", ui)
        end
    else
        reason_rows(rows, adapter)
    end
    return {heading = "Ethernet · " .. adapter.name, rows = rows}
end

local function wifi_sections(adapter, ui, lock)
    local rows = {}
    local wifi = adapter.wifi or {}
    local off = adapter.radio == false
    if off then
        rows[#rows + 1] = {icon = "network-wireless-offline", label = "Wi-Fi is off"}
    elseif adapter.state == "connected" then
        rows[#rows + 1] = {
            icon = model.level_icon(wifi.signal) or model.FULL,
            label = wifi.ssid or "Connected", value = "Connected",
        }
        if wifi.signal then
            rows[#rows + 1] = {label = "Signal", value = string.format("%d %%", wifi.signal)}
        end
        if wifi.band then
            rows[#rows + 1] = {label = "Band", value = wifi.band}
        end
        local speed = model.speed_text(wifi.rate)
        if speed then
            rows[#rows + 1] = {label = "Speed", value = speed}
        end
        if wifi.security then
            rows[#rows + 1] = {
                label = "Security", value = model.security_text(wifi.security),
                badge = model.is_secured(wifi.security) and lock or nil,
            }
        end
        address_rows(rows, adapter)
        if adapter.can_connect then
            rows[#rows + 1] = {
                label = "Disconnect", id = "disconnect:" .. adapter.name,
                on_click = function() ui.on.disconnect(adapter) end,
            }
        end
    elseif adapter.state == "connecting" then
        rows[#rows + 1] = {icon = "network-wireless-offline", label = "Connecting…"}
    else
        rows[#rows + 1] = {icon = "network-wireless-offline", label = "Not connected"}
    end
    if adapter.can_toggle then
        rows[#rows + 1] = switch_row(adapter, off and "Turn Wi-Fi on" or "Turn Wi-Fi off", ui)
    else
        reason_rows(rows, adapter)
    end
    local sections = {{heading = "Wi-Fi · " .. adapter.name, rows = rows}}

    if not off and adapter.can_scan then
        local list = {}
        local networks = model.list_networks(adapter.networks, wifi.ssid, MAX_NETWORKS)
        for _, net in ipairs(networks) do
            local usable = adapter.can_connect and (net.known or not net.enterprise)
            local value = net.band or ""
            if net.enterprise and not net.known then
                value = "Needs a login"
            elseif net.known then
                value = (value ~= "" and value .. " · " or "") .. "Saved"
            end
            list[#list + 1] = {
                icon = model.level_icon(net.signal) or "network-wireless-signal-weak",
                label = net.ssid, value = value, id = "net:" .. net.ssid,
                badge = model.is_secured(net.security) and lock or nil,
                dim = not usable,
                on_click = usable and function() ui.on.connect(adapter, net) end or nil,
            }
        end
        if #list == 0 then
            list[1] = {label = ui.scanning and "Searching…" or "No networks found", dim = true}
        end
        sections[#sections + 1] = {heading = "Available networks", rows = list}
    end
    return sections
end

-- The window: the state in a few words, then every adapter, the networks in
-- range of every Wi-Fi, and the question for a password when there is one
function view.popup(adapters, ui, lock_icon)
    local sections = {}
    if ui.message then
        sections[#sections + 1] = {rows = {{label = ui.message, id = "message"}}}
    end
    if ui.asking then
        sections[#sections + 1] = {
            heading = "Connect to " .. ui.asking.ssid,
            rows = {
                {label = "Password", entry = true, secret = true, id = "password", on_submit = ui.on.submit},
                {label = "Cancel", id = "cancel", on_click = ui.on.cancel},
            },
        }
    end
    for _, adapter in ipairs(adapters) do
        local parts = adapter.kind == "wifi" and wifi_sections(adapter, ui, lock_icon)
            or {ethernet_section(adapter, ui)}
        if #sections + #parts > MAX_SECTIONS then
            break
        end
        for _, part in ipairs(parts) do
            sections[#sections + 1] = part
        end
    end
    local panel = view.panel(adapters)
    return {
        icon = panel.icon or panel.under or "network-wired-disconnected",
        title = "Network", subtitle = summary(adapters),
        sections = sections,
    }
end

return view
