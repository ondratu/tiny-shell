-- FreeBSD, OpenBSD, NetBSD and DragonFly: what `ifconfig -a` says. It only
-- looks (the state, the addresses, the speed, the name and the signal of the
-- Wi-Fi where `ifconfig` has them); the changes are made with the tools of the
-- system.

local util = require("util")
local model = require("model")

local bsd = {}
bsd.can_change = false
bsd.can_scan = false

-- Names of adapters that are not a card: tunnels, bridges, the loopback...
local VIRTUAL = {
    "lo", "enc", "pflog", "pfsync", "tun", "tap", "bridge", "vether", "vlan",
    "epair", "vxlan", "ppp", "sl", "gif", "gre", "wg", "lagg", "carp", "ipsec",
    "bpf", "veb", "vport", "svlan", "pppoe", "trunk", "mpe", "mpw",
}

local function is_virtual(name)
    for _, prefix in ipairs(VIRTUAL) do
        -- tun0, bridge1, epair0a
        if name:sub(1, #prefix) == prefix and name:sub(#prefix + 1):match("^%d*[ab]?$") then
            return true
        end
    end
    return false
end

-- 0xffffff00 -> 24
local function prefix_of_mask(mask)
    local value = tonumber(mask)
    if not value then
        return nil
    end
    local bits = 0
    while value > 0 do
        bits = bits + (value & 1)
        value = value >> 1
    end
    return bits
end

-- A name that may be quoted: `"My Home" chan` -> My Home
local function name_after(text, key)
    local quoted = text:match("%f[%w]" .. key .. '%s+"([^"]*)"')
    if quoted then
        return quoted
    end
    return text:match("%f[%w]" .. key .. "%s+(%S+)")
end

-- One interface of `ifconfig -a`: the lines of its block
local function parse_block(name, lines)
    local text = table.concat(lines, "\n")
    local flags = text:match("flags=%x*<([^>]*)>") or ""
    local adapter = {
        name = name, ipv4 = {}, ipv6 = {},
        up = flags:find("UP", 1, true) ~= nil,
        status = text:match("\n%s*status:%s*([^\n]-)%s*\n") or text:match("\n%s*status:%s*([^\n]-)%s*$"),
        has_ether = text:find("\n%s*ether ") ~= nil or text:find("\n%s*lladdr ") ~= nil,
        media = text:match("\n%s*media:%s*([^\n]*)") or "",
    }
    for _, line in ipairs(lines) do
        local v4, rest4 = line:match("^%s*inet (%d+%.%d+%.%d+%.%d+)(.*)$")
        if v4 then
            -- 192.168.1.5/24 (NetBSD) or netmask 0xffffff00
            local prefix = rest4:match("^/(%d+)") or prefix_of_mask(rest4:match("netmask%s+(%S+)"))
            adapter.ipv4[#adapter.ipv4 + 1] = prefix and (v4 .. "/" .. prefix) or v4
        end
        local v6, rest6 = line:match("^%s*inet6 ([%x:]+)(.*)$")
        if v6 then
            -- fe80::1%em0 prefixlen 64 (the others), fe80::1%wm0/64 flags 0 (NetBSD)
            local prefix = rest6:match("prefixlen%s+(%d+)") or rest6:match("^%%?[%w_%.%-]*/(%d+)")
            adapter.ipv6[#adapter.ipv6 + 1] = prefix and (v6 .. "/" .. prefix) or v6
        end
    end
    -- the speed of a cable: (1000baseT <full-duplex>), (10Gbase-T ...)
    local number, giga = adapter.media:match("%((%d+)(G?)base")
    if number then
        adapter.speed = tonumber(number) * (giga == "G" and 1000 or 1)
    end
    -- the Wi-Fi
    local wireless = adapter.media:find("802%.11") ~= nil
    if wireless then
        adapter.kind = "wifi"
        local ssid = name_after(text, "ssid") or name_after(text, "nwid")
        adapter.ssid = model.clean(ssid)
        local mhz = tonumber(text:match("channel%s+%d+%s+%((%d+)%s*MHz"))
        adapter.band = model.band(mhz) or model.band_of_channel(tonumber(text:match("chan%a*%s+(%d+)")))
        local percent = text:match("bssid%s+%x+:%x+:%x+:%x+:%x+:%x+%s+(%d+)%%")
        adapter.signal = tonumber(percent)
        adapter.rate = tonumber(adapter.media:match("/(%d+)Mbps"))
    elseif adapter.has_ether or adapter.media:find("Ethernet", 1, true) then
        adapter.kind = "ethernet"
    end
    return adapter
end

-- The text of `ifconfig -a` -> the adapters
function bsd.parse(text)
    local blocks, current = {}, nil
    for _, line in ipairs(util.lines(text)) do
        local name = line:match("^([%w_%.%-]+):%s+flags=")
        if name then
            current = {name = name, lines = {line}}
            blocks[#blocks + 1] = current
        elseif current then
            current.lines[#current.lines + 1] = line
        end
    end
    local adapters = {}
    for _, block in ipairs(blocks) do
        if not is_virtual(block.name) then
            local info = parse_block(block.name, block.lines)
            if info.kind then
                local connected = info.status == "active" or info.status == "associated"
                local state = connected and "connected" or (info.up and "disconnected" or "disabled")
                local adapter = {
                    name = info.name, kind = info.kind, state = state, carrier = connected,
                    speed = info.speed, ipv4 = info.ipv4, ipv6 = info.ipv6,
                    can_toggle = false, can_connect = false, can_scan = false,
                    reason = "Managed by the system",
                }
                if info.kind == "wifi" and connected then
                    adapter.wifi = {ssid = info.ssid, signal = info.signal, band = info.band, rate = info.rate}
                elseif info.kind == "wifi" then
                    adapter.speed = nil
                end
                adapters[#adapters + 1] = adapter
            end
        end
    end
    return adapters
end

-- `netstat -rn`: the adapter of every default route -> 0. The column is the
-- one that the header calls Netif, Interface or Iface, so the lines may have
-- more columns after it. There is no metric: the first one wins.
function bsd.parse_default_routes(text)
    local out, column = {}, nil
    for _, line in ipairs(util.lines(text)) do
        local fields = {}
        for word in line:gmatch("%S+") do
            fields[#fields + 1] = word
        end
        if not column then
            for i, word in ipairs(fields) do
                if word == "Netif" or word == "Interface" or word == "Iface" then
                    column = i
                end
            end
        elseif fields[1] == "default" and fields[column] then
            out[fields[column]] = out[fields[column]] or 0
        end
    end
    return out
end

function bsd.read(done)
    -- /sbin is not in the PATH of every user
    tiny.exec({tiny.which("ifconfig") or "/sbin/ifconfig", "-a"}, function(result)
        if not result.ok then
            return done({})
        end
        local adapters = bsd.parse(result.stdout)
        tiny.exec({"netstat", "-rn", "-f", "inet"}, function(routes)
            if routes.ok then
                local metrics = bsd.parse_default_routes(routes.stdout)
                for _, adapter in ipairs(adapters) do
                    adapter.metric = metrics[adapter.name]
                end
            end
            done(adapters)
        end, {timeout = 5000})
    end, {timeout = 5000})
end

return bsd
