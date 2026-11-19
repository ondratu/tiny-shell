-- The plugin runs once, when the shell starts it (or the user switches it
-- on). What it needs later it asks of the loop of the shell: timers, programs.

local timer

local function greet()
    local text = tiny.setting("greeting")
    if tiny.setting("shout") then
        text = text:upper() .. "!"
    end
    tiny.log(text, "(theme: " .. tiny.theme() .. ")")
end

local function start()
    if timer then
        timer:cancel()
    end
    timer = tiny.every(tiny.setting("seconds") * 1000, greet)
end

-- The shell calls this when the user changes a setting of the plugin (or of
-- the shell). Without it the plugin would be started again.
tiny.on_config(start)

tiny.log("started, version " .. tiny.plugin.version .. " of " .. tiny.plugin.name)
greet()
start()
