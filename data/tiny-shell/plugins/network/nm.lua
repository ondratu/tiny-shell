-- Linux with NetworkManager: what sys.lua sees, and `nmcli` for the names of
-- the connections, the networks in range and the changes. Only the programs
-- of the system are used (no library), so a computer without NetworkManager
-- falls back to sys.lua.

local util = require("util")
local model = require("model")
local sys = require("sys")

local nm = {}
nm.can_change = true
nm.can_scan = true

local PERMISSION_AGE = 60 -- seconds before the permissions are asked again
local permissions, permissions_at = nil, 0
local KNOWN_AGE = 30 -- seconds before the saved networks are asked again
local known_cache, known_at = {}, 0

-- DEVICE:TYPE:STATE:CONNECTION -> device -> {state, connection}
function nm.parse_status(text)
    local out = {}
    for _, line in ipairs(util.lines(text)) do
        local f = util.split_terse(line)
        if #f >= 4 and f[1] ~= "" then
            out[f[1]] = {type = f[2], state = f[3]:match("^%S+") or "", connection = f[4] ~= "" and f[4] or nil}
        end
    end
    return out
end

-- The state of NetworkManager in the words of the plugin
local STATES = {
    connected = "connected",
    connecting = "connecting",
    deactivating = "connecting",
    disconnected = "disconnected",
    unavailable = "unavailable",
    unmanaged = "unmanaged",
}

-- IN-USE:SSID:SIGNAL:FREQ:RATE:SECURITY:DEVICE -> device -> {networks}
function nm.parse_wifi_list(text)
    local out = {}
    for _, line in ipairs(util.lines(text)) do
        local f = util.split_terse(line)
        if #f >= 7 and f[7] ~= "" then
            local security = f[6] ~= "" and f[6] or "--"
            local list = out[f[7]] or {}
            out[f[7]] = list
            list[#list + 1] = {
                ssid = model.clean(f[2]), active = f[1] == "*", signal = tonumber(f[3]),
                band = model.band(tonumber(f[4]:match("^(%d+)"))),
                rate = tonumber(f[5]:match("^([%d%.]+)")),
                security = security, enterprise = security:find("802.1X", 1, true) ~= nil,
            }
        end
    end
    return out
end

-- NAME:TYPE of the connections -> a set of the names of the Wi-Fi ones
function nm.parse_known(text)
    local known = {}
    for _, line in ipairs(util.lines(text)) do
        local f = util.split_terse(line)
        if #f >= 2 and f[2] == "802-11-wireless" and f[1] ~= "" then
            known[f[1]] = true
        end
    end
    return known
end

-- PERMISSION:VALUE -> what the user may do; "auth" is not a no, the agent of
-- the session asks
function nm.parse_permissions(text)
    local function allowed(value)
        return value ~= "no"
    end
    local raw = {}
    for _, line in ipairs(util.lines(text)) do
        local f = util.split_terse(line)
        if #f >= 2 then
            raw[f[1]:gsub("^org%.freedesktop%.NetworkManager%.", "")] = f[2]
        end
    end
    return {
        control = allowed(raw["network-control"]),
        wifi = allowed(raw["enable-disable-wifi"]),
        modify = allowed(raw["settings.modify.own"]) or allowed(raw["settings.modify.system"]),
    }
end

-- The permissions are asked again at the next look
function nm.forget_permissions()
    permissions, permissions_at = nil, 0
    known_cache, known_at = {}, 0
end

-- Why `nmcli` failed, in words (the status is its exit code)
function nm.explain(result)
    if result.ok then
        return nil
    elseif result.timed_out or result.status == 3 then
        return "It took too long"
    elseif result.status == 4 then
        return "Could not connect (is the password right?)"
    elseif result.status == 8 then
        return "NetworkManager is not running"
    elseif result.status == 10 then
        return "The network is not there any more"
    elseif result.error then
        return "nmcli: " .. result.error
    end
    return "The change did not work"
end

local function nmcli(argv, fn, timeout, input)
    local full = {"nmcli"}
    for _, a in ipairs(argv) do
        full[#full + 1] = a
    end
    tiny.exec(full, fn, {timeout = timeout or 15000, stdin = input})
end

local function ask_permissions(done)
    if permissions and os.time() - permissions_at < PERMISSION_AGE then
        return done()
    end
    nmcli({"-t", "-f", "PERMISSION,VALUE", "general", "permissions"}, function(result)
        permissions = result.ok and nm.parse_permissions(result.stdout) or {control = true, wifi = true, modify = true}
        permissions_at = os.time()
        done()
    end)
end

-- The saved Wi-Fi networks; they change rarely, so not at every look
local function ask_known(done)
    if os.time() - known_at < KNOWN_AGE then
        return done(known_cache)
    end
    nmcli({"-t", "-f", "NAME,TYPE", "connection", "show"}, function(result)
        if result.ok then
            known_cache, known_at = nm.parse_known(result.stdout), os.time()
        end
        done(known_cache)
    end)
end

-- Puts what NetworkManager knows over what the kernel says
local function merge(adapters, status, lists, known, radio)
    for _, adapter in ipairs(adapters) do
        local row = status[adapter.name]
        if row then
            adapter.connection = row.connection
            local state = STATES[row.state] or adapter.state
            if state == "unmanaged" then
                adapter.can_toggle, adapter.can_connect = false, false
                adapter.reason = "Not managed by NetworkManager"
            else
                adapter.state = state
                local control = permissions and permissions.control
                if adapter.kind == "wifi" then
                    adapter.radio = radio ~= false
                    adapter.can_toggle = control and permissions.wifi
                    adapter.can_connect = control and adapter.radio and permissions.modify
                else
                    adapter.can_toggle = control
                    adapter.can_connect = control
                end
                adapter.reason = (not control and "You may not change the network")
                    or (adapter.kind == "wifi" and not permissions.wifi and "You may not switch the Wi-Fi")
                    or nil
                adapter.can_scan = adapter.kind == "wifi"
            end
        else
            adapter.can_toggle, adapter.can_connect = false, false
            adapter.reason = "Not managed by NetworkManager"
        end
        if adapter.kind == "wifi" then
            local networks = lists[adapter.name] or {}
            for _, net in ipairs(networks) do
                net.known = known[net.ssid] == true
            end
            adapter.networks = networks
            if adapter.state == "connected" then
                local current = adapter.wifi or {}
                for _, net in ipairs(networks) do
                    if net.active then
                        current = {ssid = net.ssid, signal = net.signal, band = net.band,
                                   rate = net.rate, security = net.security}
                    end
                end
                current.ssid = current.ssid or adapter.connection
                adapter.wifi = current
            elseif adapter.state ~= "connecting" then
                adapter.wifi = nil
            end
        end
    end
end

-- Calls `done(adapters)`
function nm.read(done)
    sys.read(function(adapters)
        if #adapters == 0 then
            return done(adapters)
        end
        nmcli({"-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "device"}, function(result)
            if not result.ok then
                for _, adapter in ipairs(adapters) do
                    adapter.can_toggle, adapter.can_connect, adapter.can_scan = false, false, false
                    adapter.reason = nm.explain(result)
                end
                return done(adapters)
            end
            local status = nm.parse_status(result.stdout)
            local has_wifi = false
            for _, adapter in ipairs(adapters) do
                has_wifi = has_wifi or adapter.kind == "wifi"
            end
            ask_permissions(function()
                if not has_wifi then
                    merge(adapters, status, {}, {}, true)
                    return done(adapters)
                end
                nmcli({"-t", "-f", "IN-USE,SSID,SIGNAL,FREQ,RATE,SECURITY,DEVICE",
                       "device", "wifi", "list", "--rescan", "no"}, function(list)
                    ask_known(function(known)
                        nmcli({"-t", "radio", "wifi"}, function(radio)
                            merge(adapters, status,
                                  list.ok and nm.parse_wifi_list(list.stdout) or {},
                                  known,
                                  not (radio.ok and radio.stdout:find("disabled", 1, true)))
                            done(adapters)
                        end)
                    end)
                end)
            end)
        end)
    end, {iw = false})
end

-- Asks for a new look at the networks in range; the answer is in the next read
function nm.scan(adapter, done)
    nmcli({"device", "wifi", "rescan", "ifname", adapter.name}, function()
        done()
    end)
end

-- Turns an adapter on or off: the radio for a Wi-Fi, the connection for the
-- other one. Calls `done(error_text_or_nil)`.
function nm.toggle(adapter, enable, done)
    local argv = adapter.kind == "wifi"
        and {"radio", "wifi", enable and "on" or "off"}
        or {"device", enable and "connect" or "disconnect", adapter.name}
    nmcli(argv, function(result)
        permissions_at = 0
        done(nm.explain(result))
    end, 40000)
end

function nm.disconnect(adapter, done)
    nmcli({"device", "disconnect", adapter.name}, function(result)
        done(nm.explain(result))
    end, 20000)
end

-- Connects to a network: a saved one by its name, a new one with the password
-- that is given on the standard input (not in the arguments: `ps` shows them)
function nm.connect(adapter, network, password, done)
    if network.ssid:sub(1, 1) == "-" then
        return done("A name that starts with a dash is not supported")
    end
    local function finished(result)
        known_at = 0 -- a new connection is saved
        done(nm.explain(result))
    end
    if network.known then
        return nmcli({"-w", "30", "connection", "up", "id", network.ssid, "ifname", adapter.name},
                     finished, 40000)
    end
    if model.is_secured(network.security) and (password or "") == "" then
        return done("A password is needed")
    end
    local argv = {"--ask", "-w", "30", "device", "wifi", "connect", network.ssid, "ifname", adapter.name}
    nmcli(argv, function(result)
        if result.ok then
            return finished(result)
        end
        -- the profile of a network that did not connect (a wrong password) is
        -- not kept: it would be shown as saved and never ask again
        nmcli({"connection", "delete", "id", network.ssid}, function()
            finished(result)
        end)
    end, 40000, model.is_secured(network.security) and (password .. "\n") or nil)
end

return nm
