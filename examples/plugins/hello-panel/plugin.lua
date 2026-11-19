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
-- room, so the other widgets do not move when the text changes
button = tiny.panel.button{
    text = shown(),
    sample = "A long greeting 00:00:00",
    on_click = function()
        clicks = clicks + 1
        button:set_text(shown())
    end,
}

tiny.on_config(function()
    button:set_text(shown())
end)
