-- What the plugin knows about the network, and the rules that do not depend on
-- the system: the icons of the signal, the bands, the speed, the main adapter.
--
-- An adapter is a table made by a backend:
--   name, kind = "ethernet" | "wifi"
--   state = "connected" | "connecting" | "disconnected" | "disabled" | "unavailable"
--   carrier = true | false          (the cable is in / the Wi-Fi is associated)
--   speed = Mb/s | nil
--   ipv4 = {"10.0.0.2/24"}, ipv6 = {...}
--   metric = the metric of its default route, nil without one
--   connection = the name of the connection (NetworkManager), nil
--   can_toggle, can_connect = true | false
--   reason = why not (text), nil
--   radio = false when the radio of a Wi-Fi adapter is off
--   can_scan = true when the backend lists the networks in range
--   wifi = {ssid, signal = 0..100, band = "5 GHz", rate = Mb/s, security}
--   networks = {{ssid, signal, band, security, known, enterprise}}

local model = {}

-- A name that a stranger chose (a Wi-Fi): without the control characters
function model.clean(text)
    return text and (text:gsub("%c", "?")) or nil
end

-- The icon that is drawn first, dimmed, under the level of the signal
model.FULL = "network-wireless-signal-excellent"

-- The icon of a signal of 0..100; nil when there is none
function model.level_icon(signal)
    if not signal then
        return nil
    end
    if signal >= 80 then
        return "network-wireless-signal-excellent"
    elseif signal >= 55 then
        return "network-wireless-signal-good"
    elseif signal >= 30 then
        return "network-wireless-signal-ok"
    elseif signal >= 5 then
        return "network-wireless-signal-weak"
    end
    return nil
end

-- dBm of a radio -> 0..100, as NetworkManager does it
function model.dbm_to_percent(dbm)
    return math.max(0, math.min(100, math.floor(2 * (dbm + 100) + 0.5)))
end

-- The band of a frequency in MHz
function model.band(mhz)
    if not mhz then
        return nil
    elseif mhz >= 2400 and mhz < 2500 then
        return "2.4 GHz"
    elseif mhz >= 4900 and mhz < 5900 then
        return "5 GHz"
    elseif mhz >= 5925 and mhz <= 7125 then
        return "6 GHz"
    end
    return nil
end

-- The band of a channel number, for a system that does not tell the frequency
function model.band_of_channel(channel)
    if not channel then
        return nil
    elseif channel >= 1 and channel <= 14 then
        return "2.4 GHz"
    elseif channel >= 32 and channel <= 177 then
        return "5 GHz"
    end
    return nil
end

-- "1 Gb/s", "100 Mb/s", "2.5 Gb/s"; nil for what is not a speed
function model.speed_text(mbps)
    if type(mbps) ~= "number" or mbps <= 0 then
        return nil
    end
    if mbps >= 1000 then
        local g = mbps / 1000
        return (g == math.floor(g) and string.format("%d", g) or string.format("%.1f", g)) .. " Gb/s"
    end
    return string.format("%d Mb/s", math.floor(mbps + 0.5))
end

-- Whether the security that NetworkManager shows ("WPA2", "--", "") needs a key
function model.is_secured(security)
    return security ~= nil and security ~= "" and security ~= "--"
end

function model.security_text(security)
    return model.is_secured(security) and security or "Open"
end

-- The adapter that the panel shows: the one that is connected and has the
-- default route (the lowest metric), else the first one that is connected
function model.primary(adapters)
    local best
    for _, adapter in ipairs(adapters) do
        if adapter.state == "connected" then
            if not best then
                best = adapter
            elseif adapter.metric and (not best.metric or adapter.metric < best.metric) then
                best = adapter
            end
        end
    end
    return best
end

-- The networks in range: one for every name (the strongest), the connected
-- one is not in the list, the strongest first; at most `limit`
function model.list_networks(networks, connected_ssid, limit)
    local by_name, order = {}, {}
    for _, net in ipairs(networks or {}) do
        local ssid = net.ssid
        if ssid and ssid ~= "" and ssid ~= connected_ssid then
            local seen = by_name[ssid]
            if not seen then
                by_name[ssid] = net
                order[#order + 1] = ssid
            elseif (net.signal or 0) > (seen.signal or 0) then
                net.known = net.known or seen.known
                by_name[ssid] = net
            else
                seen.known = seen.known or net.known
            end
        end
    end
    local list = {}
    for _, ssid in ipairs(order) do
        list[#list + 1] = by_name[ssid]
    end
    table.sort(list, function(a, b)
        if (a.signal or 0) ~= (b.signal or 0) then
            return (a.signal or 0) > (b.signal or 0)
        end
        return a.ssid < b.ssid
    end)
    while #list > limit do
        list[#list] = nil
    end
    return list
end

return model
