-- The reader of NetBSD: envstat lists the sensors by device, "[acpibat0]"
-- with "present:", "charging:" and "charge: ... Ah (96.20%)", and
-- "[acpiacad0]" with "connected:".

local M = {}

-- The text of `envstat` -> {percent, state} or nil without a battery. The
-- percentage is the one that envstat prints in the brackets, taken from the
-- last full capacity. The state: charging, full (on mains and not charging)
-- or discharging.
function M.parse(text)
    local total, count, charging, mains = 0, 0, false, false
    local device, present, percent, on_charge
    local function finish_device()
        if device and device:find("^acpibat") and present and percent then
            total, count = total + percent, count + 1
            charging = charging or on_charge
        end
    end
    for line in text:gmatch("[^\n]+") do
        local name = line:match("^%[([^%]]+)%]")
        if name then
            finish_device()
            device, present, percent, on_charge = name, false, nil, false
        elseif device then
            local key, value = line:match("^%s*([%a ]-):%s+(.-)%s*$")
            if key == "present" then
                present = value == "TRUE"
            elseif key == "charging" then
                on_charge = value == "TRUE"
            elseif key == "charge" then
                percent = tonumber(value:match("%(([%d.]+)%%%)"))
            elseif key == "connected" and device:find("^acpiacad") then
                mains = mains or value == "TRUE"
            end
        end
    end
    finish_device()
    if count == 0 then
        return nil
    end
    local state = "discharging"
    if charging then
        state = "charging"
    elseif mains then
        state = "full"
    end
    return {percent = math.floor(total / count + 0.5), state = state}
end

return M
