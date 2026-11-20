-- Small helpers that the backends share.

local util = {}

function util.trim(text)
    return text and (text:match("^%s*(.-)%s*$")) or nil
end

-- The text of a file without the blanks around it, or nil
function util.read(path)
    return util.trim((tiny.read_file(path)))
end

function util.is_dir(path)
    return tiny.list_dir(path) ~= nil
end

-- The lines of a text
function util.lines(text)
    local list = {}
    for line in ((text or "") .. "\n"):gmatch("(.-)\r?\n") do
        list[#list + 1] = line
    end
    return list
end

-- The fields of a line of the terse output of nmcli (`-t`): the fields are
-- separated by a colon, a colon or a backslash in a value has a backslash
-- before it
function util.split_terse(line)
    local fields, current = {}, {}
    local i = 1
    while i <= #line do
        local c = line:sub(i, i)
        if c == "\\" and i < #line then
            current[#current + 1] = line:sub(i + 1, i + 1)
            i = i + 2
        elseif c == ":" then
            fields[#fields + 1] = table.concat(current)
            current = {}
            i = i + 1
        else
            current[#current + 1] = c
            i = i + 1
        end
    end
    fields[#fields + 1] = table.concat(current)
    return fields
end

-- Calls `fn(item, next)` for every item one after the other; `finish()` at the end
function util.each(list, fn, finish)
    local index = 0
    local function step()
        index = index + 1
        if index > #list then
            return finish()
        end
        fn(list[index], step)
    end
    step()
end

-- The first address of a family that is worth showing (not link-local) and
-- the ones after it, at most `limit`
function util.pick_addresses(list, limit)
    local global, local_only = {}, {}
    for _, address in ipairs(list or {}) do
        if address:find("^fe80:") or address:find("^169%.254%.") then
            local_only[#local_only + 1] = address
        else
            global[#global + 1] = address
        end
    end
    local use = #global > 0 and global or local_only
    local out = {}
    for i = 1, math.min(limit, #use) do
        out[i] = use[i]
    end
    return out
end

return util
