-- The weather in the panel, from the free service open-meteo.com (no key, no
-- account). It asks twice at most: for the place of the town by its name (once
-- for a name) and for the weather of that place. A click shows the forecast
-- for the next day, and the next one, and then the weather now again.
--
-- The programs are started with tiny.exec: the shell never waits for the
-- network.

local widget
local view = 0        -- 0 is now, 1 and 2 are the next days
local place          -- {name =, latitude =, longitude =} of `place_for`
local place_for = nil
local weather        -- the answer of the service
local message = "..." -- what is shown while there is no weather
local timer
local generation = 0 -- a late answer for an old setting is dropped

local WORDS = {
    [0] = "Clear", [1] = "Clear", [2] = "Cloudy", [3] = "Overcast",
    [45] = "Fog", [48] = "Fog",
    [51] = "Drizzle", [53] = "Drizzle", [55] = "Drizzle",
    [56] = "Drizzle", [57] = "Drizzle",
    [61] = "Rain", [63] = "Rain", [65] = "Rain", [66] = "Rain", [67] = "Rain",
    [71] = "Snow", [73] = "Snow", [75] = "Snow", [77] = "Snow",
    [80] = "Showers", [81] = "Showers", [82] = "Showers",
    [85] = "Snow", [86] = "Snow",
    [95] = "Storm", [96] = "Storm", [99] = "Storm",
}

local function word(code)
    return WORDS[code] or "?"
end

local function degrees(value)
    local unit = tiny.setting("units") == "imperial" and "F" or "C"
    return string.format("%d°%s", math.floor(value + 0.5), unit)
end

local function weekday(date)
    local year, month, day = date:match("^(%d+)-(%d+)-(%d+)")
    if not year then
        return "?"
    end
    return os.date("%a", os.time{year = year, month = month, day = day, hour = 12})
end

local function text()
    if not weather then
        return message
    end
    if view == 0 then
        local now = weather.current
        return degrees(now.temperature_2m) .. " " .. word(now.weather_code)
    end
    local daily = weather.daily
    local low, high = daily.temperature_2m_min[view + 1], daily.temperature_2m_max[view + 1]
    return string.format("%s %d/%s %s", weekday(daily.time[view + 1]),
        math.floor(low + 0.5), degrees(high), word(daily.weather_code[view + 1]))
end

local function redraw()
    local shown = text()
    if widget then
        widget:set_text(shown)
    end
end

local function problem(what)
    weather = nil
    message = what
    redraw()
end

-- The answer of curl as JSON, or nothing (the problem is shown)
local function decode(result, what)
    if not result.ok then
        problem(result.timed_out and "No answer" or "No network")
        return nil
    end
    local data, error = tiny.json.decode(result.stdout)
    if type(data) ~= "table" then
        tiny.log(what .. ": " .. tostring(error))
        problem("Bad answer")
        return nil
    end
    return data
end

local function encode(text)
    return (text:gsub("[^%w%-_%.~]", function(c) return string.format("%%%02X", c:byte()) end))
end

local function fetch(url, done)
    tiny.exec({"curl", "-fsS", "--max-time", "20", url}, done, {timeout = 25000})
end

local function forecast(mine)
    local url = string.format(
        "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s"
        .. "&current=temperature_2m,weather_code"
        .. "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        .. "&forecast_days=3&timezone=auto&temperature_unit=%s",
        place.latitude, place.longitude,
        tiny.setting("units") == "imperial" and "fahrenheit" or "celsius")
    fetch(url, function(result)
        if mine ~= generation then
            return
        end
        local data = decode(result, "forecast")
        if data and data.current and data.daily then
            weather = data
            redraw()
        elseif data then
            problem("Bad answer")
        end
    end)
end

local function update()
    -- also when there is nothing to ask: an answer that is on its way is old
    generation = generation + 1
    local mine = generation
    local location = tiny.setting("location")
    if location == "" then
        return problem("Set a location")
    end
    if not tiny.which("curl") then
        return problem("No curl")
    end
    if place_for == location and place then
        return forecast(mine)
    end
    fetch("https://geocoding-api.open-meteo.com/v1/search?count=1&format=json&name="
        .. encode(location), function(result)
        if mine ~= generation then
            return
        end
        local data = decode(result, "geocoding")
        if not data then
            return
        end
        local found = data.results and data.results[1]
        if not found or not found.latitude then
            return problem("Unknown town")
        end
        place = {name = found.name, latitude = found.latitude, longitude = found.longitude}
        place_for = location
        forecast(mine)
    end)
end

local function start()
    if timer then
        timer:cancel()
    end
    timer = tiny.every(tiny.setting("interval") * 60 * 1000, update)
end

widget = tiny.panel.button{
    text = message,
    sample = "Wed -99/-99°F Overcast",
    on_click = function()
        if weather then
            view = (view + 1) % 3
            redraw()
        end
    end,
}

local function settings()
    return table.concat({tiny.setting("location"), tiny.setting("units"), tiny.setting("interval")}, "|")
end
local known = settings()

-- the shell tells every change of its settings: only ours matter
tiny.on_config(function()
    if settings() == known then
        return
    end
    known = settings()
    start()
    weather = nil
    message = "..."
    redraw()
    update()
end)
start()
update()
