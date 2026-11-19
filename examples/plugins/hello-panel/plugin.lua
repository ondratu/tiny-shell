local clicks = 0
local button

local function shown()
    local text = tiny.setting("text")
    if clicks > 0 then
        if tiny.setting("format") == "time" then
            text = text .. " " .. os.date("%H:%M:%S")
        else
            text = text .. " " .. clicks
        end
    end
    return text
end

-- `sample` is the longest text of the button: the panel gives it that much
-- room, so the other widgets do not move when the text changes. `icon` is
-- a symbolic icon of the shell (the list is in data/tiny-shell/icons).
button = tiny.panel.button{
    text = shown(),
    sample = "A long greeting 00:00:00",
    icon = "emblem-shared",
    on_click = function()
        clicks = clicks + 1
        button:set_text(shown())
        -- the window under the button: a header and lines of details
        button:popup{
            icon = "emblem-shared",
            title = "Hello, panel",
            subtitle = "A plugin that counts clicks",
            value = tostring(clicks),
            sections = {
                {heading = "The last click", rows = {
                    {label = "Time", value = os.date("%H:%M:%S")},
                    {label = "Clicks so far", value = tostring(clicks)},
                }},
            },
        }
    end,
}

tiny.on_config(function()
    button:set_text(shown())
end)
