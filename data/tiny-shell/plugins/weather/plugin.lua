-- The weather in the panel, from the free service open-meteo.com (no key, no
-- account): the symbol and the temperature. A click opens a window with the
-- weather now (the feeling, the humidity, the wind, the pressure) and the
-- forecast for the next days. Nothing changes by itself.
--
-- It asks twice at most: for the place of the town by its name (once for a
-- name) and for the weather of that place. The programs are started with
-- tiny.exec: the shell never waits for the network.

local FORECAST_DAYS = 5 -- the days after today that the window shows

local widget
local place          -- {name =, latitude =, longitude =} of `place_for`
local place_for = nil
local weather        -- the answer of the service
local message = "..." -- what is said while there is no weather
local timer
local generation = 0 -- a late answer for an old setting is dropped

-- the code of the weather (WMO) -> {the name of the icon, the words}
local KINDS = {
    [0] = {"weather-clear", "Clear sky"},
    [1] = {"weather-few-clouds", "Mainly clear"},
    [2] = {"weather-few-clouds", "Partly cloudy"},
    [3] = {"weather-overcast", "Overcast"},
    [45] = {"weather-fog", "Fog"}, [48] = {"weather-fog", "Freezing fog"},
    [51] = {"weather-showers", "Light drizzle"}, [53] = {"weather-showers", "Drizzle"},
    [55] = {"weather-showers", "Heavy drizzle"},
    [56] = {"weather-showers", "Freezing drizzle"}, [57] = {"weather-showers", "Freezing drizzle"},
    [61] = {"weather-showers", "Light rain"}, [63] = {"weather-showers", "Rain"},
    [65] = {"weather-showers", "Heavy rain"},
    [66] = {"weather-showers", "Freezing rain"}, [67] = {"weather-showers", "Freezing rain"},
    [71] = {"weather-snow", "Light snow"}, [73] = {"weather-snow", "Snow"},
    [75] = {"weather-snow", "Heavy snow"}, [77] = {"weather-snow", "Snow grains"},
    [80] = {"weather-showers", "Rain showers"}, [81] = {"weather-showers", "Rain showers"},
    [82] = {"weather-showers", "Violent showers"},
    [85] = {"weather-snow", "Snow showers"}, [86] = {"weather-snow", "Snow showers"},
    [95] = {"weather-storm", "Thunderstorm"}, [96] = {"weather-storm", "Thunderstorm with hail"},
    [99] = {"weather-storm", "Thunderstorm with hail"},
}

local function kind(code, night)
    local found = KINDS[code] or {"weather-overcast", "Unknown"}
    local icon = found[1]
    if night and (icon == "weather-clear" or icon == "weather-few-clouds") then
        icon = icon .. "-night"
    end
    return icon, found[2]
end

local function imperial()
    return tiny.setting("units") == "imperial"
end

-- a missing value is a JSON null (not a number): shown as a dash
local function round(value)
    return math.floor(value + 0.5)
end

local function degrees(value)
    return type(value) == "number" and string.format("%d°", round(value)) or "-"
end

local function with_unit(value)
    return degrees(value) .. (type(value) == "number" and (imperial() and "F" or "C") or "")
end

local function weekday(date)
    local year, month, day = date:match("^(%d+)-(%d+)-(%d+)")
    if not year then
        return "?"
    end
    return os.date("%A", os.time{year = year, month = month, day = day, hour = 12})
end

local function number(value, digits)
    return type(value) == "number" and string.format("%." .. digits .. "f", value) or "-"
end

local function now_icon()
    local current = weather.current
    return kind(current.weather_code, current.is_day == 0)
end

-- the panel: the symbol and the temperature
local function redraw()
    if not widget then
        return
    end
    if not weather then
        widget:set_icon("weather-overcast")
        widget:set_text("")
        return
    end
    widget:set_icon((now_icon()))
    widget:set_text(degrees(weather.current.temperature_2m))
end

-- the window: the weather now, the details of today, the next days
local function details()
    local current, daily = weather.current, weather.daily
    local icon, words = now_icon()
    local rows = {
        {label = "Feels like", value = with_unit(current.apparent_temperature)},
        {label = "Humidity", value = number(current.relative_humidity_2m, 0) .. " %"},
        {label = "Wind", value = number(current.wind_speed_10m, 0) .. (imperial() and " mph" or " km/h")},
        {label = "Pressure", value = number(current.pressure_msl, 0) .. " hPa"},
        {label = "Precipitation", value = number(current.precipitation, 1) .. (imperial() and " in" or " mm")},
        {label = "Low / high", value = degrees(daily.temperature_2m_min[1]) .. " / " .. degrees(daily.temperature_2m_max[1])},
    }
    local days = {}
    for i = 2, FORECAST_DAYS + 1 do
        if type(daily.time[i]) == "string" then
            local day_icon = kind(daily.weather_code[i])
            days[#days + 1] = {
                icon = day_icon,
                label = weekday(daily.time[i]),
                value = degrees(daily.temperature_2m_min[i]) .. " / " .. degrees(daily.temperature_2m_max[i]),
            }
        end
    end
    return {
        icon = icon, title = place.name, subtitle = words,
        value = with_unit(current.temperature_2m),
        sections = {
            {heading = "Today", rows = rows},
            {heading = "Forecast", rows = days},
        },
    }
end

local function open_window()
    if weather then
        widget:popup(details())
    else
        widget:popup{icon = "weather-overcast", title = "Weather", subtitle = message}
    end
end

-- the last weather stays in the panel when a refresh fails; the window says why
local function problem(what)
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
        .. "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
        .. "precipitation,weather_code,pressure_msl,wind_speed_10m,is_day"
        .. "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        .. "&forecast_days=%d&timezone=auto&temperature_unit=%s"
        .. "&wind_speed_unit=%s&precipitation_unit=%s",
        place.latitude, place.longitude, FORECAST_DAYS + 1,
        imperial() and "fahrenheit" or "celsius",
        imperial() and "mph" or "kmh", imperial() and "inch" or "mm")
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
    text = "", sample = "-99°",
    icon = "weather-overcast",
    on_click = open_window,
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
