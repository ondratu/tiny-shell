-- The battery in the panel. The charge is read where the system keeps it:
-- /sys/class/power_supply on Linux, sysctl on FreeBSD, apm on OpenBSD. A
-- computer without a battery shows nothing.
--
-- info = {percent = 0..100, state = "charging" | "discharging" | "full"}

local widget
local current -- the last info, nil without a battery
local interval = tiny.setting("interval")

local function trim(text)
    return text and (text:match("^%s*(.-)%s*$")) or nil
end

local function number_in(path)
    return tonumber(trim(tiny.read_file(path)))
end

-- Linux: the batteries of the computer (not the ones of a mouse or a pad)
local function read_linux(done)
    local base = "/sys/class/power_supply/"
    local names = tiny.list_dir(base)
    if not names then
        return done(nil)
    end
    local total, count, state = 0, 0, "full"
    for _, name in ipairs(names) do
        local dir = base .. name .. "/"
        local kind = trim(tiny.read_file(dir .. "type"))
        local scope = trim(tiny.read_file(dir .. "scope"))
        local percent = number_in(dir .. "capacity")
        if kind == "Battery" and scope ~= "Device" and percent then
            total, count = total + percent, count + 1
            local status = trim(tiny.read_file(dir .. "status"))
            if status == "Charging" then
                state = "charging"
            elseif status == "Discharging" and state ~= "charging" then
                state = "discharging"
            end
        end
    end
    if count == 0 then
        return done(nil)
    end
    done({percent = math.floor(total / count + 0.5), state = state})
end

-- FreeBSD: hw.acpi.battery.life (percent) and .state (1 discharging,
-- 2 charging), -1 when the computer has no battery
local function read_freebsd(done)
    tiny.exec({"sysctl", "-n", "hw.acpi.battery.life", "hw.acpi.battery.state"},
        function(result)
            local life, state = result.stdout:match("^(%-?%d+)%s+(%d+)")
            life, state = tonumber(life), tonumber(state)
            if not result.ok or not life or life < 0 then
                return done(nil)
            end
            local kind = "full"
            if state and state & 2 ~= 0 then
                kind = "charging"
            elseif state and state & 1 ~= 0 then
                kind = "discharging"
            end
            done({percent = life, state = kind})
        end)
end

-- OpenBSD: apm prints "Battery state: high, 85% remaining, ..." and
-- "AC adapter state: connected"
local function read_openbsd(done)
    tiny.exec({"apm"}, function(result)
        local percent = tonumber(result.stdout:match("(%d+)%% remaining"))
        if not result.ok or not percent then
            return done(nil)
        end
        local kind = "discharging"
        if result.stdout:find("AC adapter state: connected", 1, true) then
            kind = percent >= 100 and "full" or "charging"
        end
        done({percent = percent, state = kind})
    end)
end

local readers = {
    linux = read_linux,
    freebsd = read_freebsd,
    dragonfly = read_freebsd,
    openbsd = read_openbsd,
}
local read = readers[tiny.platform]

local function level_icon(info)
    local p = info.percent
    local level = p >= 85 and "full" or p >= 50 and "good" or p >= 15 and "low" or "empty"
    if info.state == "charging" then
        return "battery-" .. level .. "-charging"
    end
    return "battery-" .. level
end

local function text_of(info)
    return tiny.setting("percent") and info.percent .. "%" or ""
end

local descriptions = {
    charging = "Charging",
    discharging = "On battery",
    full = "Fully charged",
}

-- The window that a click opens: the state in words and the number
local function open_details()
    if not current then
        return
    end
    widget:popup{
        icon = level_icon(current),
        title = descriptions[current.state] or "Battery",
        subtitle = current.state == "discharging" and current.percent < 15 and "Connect the charger" or "",
        value = current.percent .. "%",
    }
end

local function show(info)
    current = info
    if not info then
        if widget then
            widget:close_popup()
            widget:set_visible(false)
        end
        return
    end
    local icon = level_icon(info)
    local under = info.state ~= "charging" and "battery-full" or nil
    local text = text_of(info)
    if not widget then
        -- made when there is a battery, as wide as the widest text
        widget = tiny.panel.button{
            text = text, sample = "100%",
            icon = icon, icon_under = under, on_click = open_details,
        }
    else
        widget:set_icon(icon, under)
        widget:set_text(text)
        widget:set_visible(true)
    end
end

local function update()
    read(show)
end

local timer
local function start()
    if timer then
        timer:cancel()
    end
    interval = tiny.setting("interval")
    timer = tiny.every(interval * 1000, update)
end

if not read then
    tiny.log("no battery reader for " .. tiny.platform)
else
    -- a new setting changes the text and the pace without a new start
    tiny.on_config(function()
        start()
        update()
    end)
    start()
    update()
end
