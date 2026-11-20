-- Linux without any helper: what the kernel says in /sys and /proc, the
-- addresses from `ip`, and `iw` for the name of the Wi-Fi when it is installed.
-- It only looks: the changes are made by another backend (nm.lua).

local util = require("util")
local model = require("model")

local sys = {}

sys.can_change = false
sys.can_scan = false
-- where the kernel tells about the adapters (a test points them elsewhere)
sys.root = "/sys/class/net/"
sys.wireless_file = "/proc/net/wireless"

-- The adapters that are a real Ethernet or Wi-Fi card (not a bridge, a
-- virtual cable, a tunnel or the loopback): they have a `device`
function sys.devices()
    local found = {}
    for _, name in ipairs(tiny.list_dir(sys.root) or {}) do
        local dir = sys.root .. name
        if util.is_dir(dir .. "/device") then
            local kind
            if util.is_dir(dir .. "/wireless") or util.is_dir(dir .. "/phy80211") then
                kind = "wifi"
            elseif util.read(dir .. "/type") == "1" then
                kind = "ethernet"
            end
            if kind then
                local flags = tonumber(util.read(dir .. "/flags") or "")
                local speed = tonumber(util.read(dir .. "/speed") or "")
                found[#found + 1] = {
                    name = name, kind = kind,
                    up = flags ~= nil and flags & 1 ~= 0,
                    carrier = util.read(dir .. "/carrier") == "1",
                    speed = speed and speed > 0 and speed or nil,
                }
            end
        end
    end
    return found
end

-- `ip -j addr`: name -> {ipv4 = {"a/24"}, ipv6 = {...}}
function sys.parse_addresses(text)
    local data = tiny.json.decode(text or "")
    local out = {}
    if type(data) ~= "table" then
        return out
    end
    for _, link in ipairs(data) do
        if type(link) == "table" and type(link.ifname) == "string" then
            local v4, v6 = {}, {}
            for _, info in ipairs(type(link.addr_info) == "table" and link.addr_info or {}) do
                local address = info["local"]
                if type(address) == "string" then
                    local with_prefix = string.format("%s/%d", address, tonumber(info.prefixlen) or 0)
                    if info.family == "inet" then
                        v4[#v4 + 1] = with_prefix
                    elseif info.family == "inet6" then
                        v6[#v6 + 1] = with_prefix
                    end
                end
            end
            out[link.ifname] = {ipv4 = v4, ipv6 = v6}
        end
    end
    return out
end

-- `ip -j route show default`: name -> the lowest metric of a default route
function sys.parse_routes(text)
    local data = tiny.json.decode(text or "")
    local out = {}
    if type(data) ~= "table" then
        return out
    end
    for _, route in ipairs(data) do
        if type(route) == "table" and type(route.dev) == "string" then
            local metric = tonumber(route.metric) or 0
            if not out[route.dev] or metric < out[route.dev] then
                out[route.dev] = metric
            end
        end
    end
    return out
end

-- /proc/net/wireless: name -> the quality of the link, 0..100
function sys.parse_wireless(text)
    local out = {}
    for _, line in ipairs(util.lines(text)) do
        local name, link = line:match("^%s*([^%s:|]+):%s+%S+%s+(%-?%d+)")
        if name and tonumber(link) then
            out[name] = math.max(0, math.min(100, math.floor(tonumber(link) * 100 / 70 + 0.5)))
        end
    end
    return out
end

-- `iw dev X link`: {ssid, band, rate, signal}, nil when not connected
function sys.parse_iw(text)
    if not text or text:find("Not connected", 1, true) then
        return nil
    end
    local ssid = text:match("\n%s*SSID: ([^\n]*)")
    if not ssid then
        return nil
    end
    local freq = tonumber(text:match("\n%s*freq: (%d+)"))
    local dbm = tonumber(text:match("\n%s*signal: (%-?%d+)"))
    local rate = tonumber(text:match("\n%s*tx bitrate: ([%d%.]+)"))
    return {
        ssid = model.clean(ssid), band = model.band(freq), rate = rate,
        signal = dbm and model.dbm_to_percent(dbm) or nil,
    }
end

-- Ethernet before Wi-Fi, then by name
function sys.sort(adapters)
    table.sort(adapters, function(a, b)
        if a.kind ~= b.kind then
            return a.kind == "ethernet"
        end
        return a.name < b.name
    end)
end

local function run(argv, fn)
    if not tiny.which(argv[1]) then
        return fn({ok = false, stdout = ""})
    end
    tiny.exec(argv, fn, {timeout = 5000})
end

-- Calls `done(adapters)`. `options.iw = false` leaves the names of the
-- Wi-Fi to another program.
function sys.read(done, options)
    local use_iw = not options or options.iw ~= false
    local devices = sys.devices()
    if #devices == 0 then
        return done({})
    end
    local quality = sys.parse_wireless(tiny.read_file(sys.wireless_file) or "")
    local addresses, routes, details = {}, {}, {}
    run({"ip", "-j", "addr"}, function(result)
        addresses = result.ok and sys.parse_addresses(result.stdout) or {}
        run({"ip", "-j", "route", "show", "default"}, function(result_routes)
            routes = result_routes.ok and sys.parse_routes(result_routes.stdout) or {}
            local connected_wifi = {}
            for _, device in ipairs(devices) do
                if use_iw and device.kind == "wifi" and device.carrier then
                    connected_wifi[#connected_wifi + 1] = device
                end
            end
            util.each(connected_wifi, function(device, next)
                run({"iw", "dev", device.name, "link"}, function(r)
                    details[device.name] = r.ok and sys.parse_iw(r.stdout) or nil
                    next()
                end)
            end, function()
                local adapters = {}
                for _, device in ipairs(devices) do
                    local ips = addresses[device.name] or {ipv4 = {}, ipv6 = {}}
                    local adapter = {
                        name = device.name, kind = device.kind,
                        state = device.carrier and "connected" or (device.up and "disconnected" or "disabled"),
                        carrier = device.carrier, speed = device.speed,
                        ipv4 = ips.ipv4, ipv6 = ips.ipv6, metric = routes[device.name],
                        can_toggle = false, can_connect = false,
                        reason = "Managed by the system",
                        can_scan = false,
                    }
                    if device.kind == "wifi" and device.carrier then
                        local info = details[device.name] or {}
                        adapter.wifi = {
                            ssid = info.ssid, band = info.band, rate = info.rate,
                            signal = info.signal or quality[device.name],
                        }
                    end
                    adapters[#adapters + 1] = adapter
                end
                sys.sort(adapters)
                done(adapters)
            end)
        end)
    end)
end

return sys
