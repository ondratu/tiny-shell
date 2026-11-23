-- What does not depend on where the numbers come from: the parsers of what
-- the systems print, the percentages, and the text of the panel. The readers
-- (linux.lua, bsd.lua) give the text, this file makes numbers of it, so a
-- test can look at every system without being on it.
--
-- A processor sample is {user, system, idle, total}: counters that only grow;
-- the load is the difference of two of them. Memory and swap are
-- {total, used} in bytes. The network is {rx, tx, adapters}: the bytes that
-- came and went since the start of the system, all the adapters together and
-- each of them (adapters = {name = {rx, tx}}).

local model = {}

local KIB = 1024

-- every whole number in a text
local function numbers(text)
    local list = {}
    for digits in (text or ""):gmatch("%d+") do
        list[#list + 1] = tonumber(digits)
    end
    return list
end

local function clamp(value, low, high)
    return math.max(low, math.min(high, value))
end

-- "1124M", "6904K", "1.5G" -> bytes
function model.parse_size(text)
    local number, unit = tostring(text or ""):match("^([%d%.]+)(%a?)")
    number = tonumber(number)
    if not number then
        return nil
    end
    local power = ({K = 1, M = 2, G = 3, T = 4})[unit:upper()] or 0
    return number * KIB ^ power
end

-- Processor ---------------------------------------------------------------

-- /proc/stat: "cpu  user nice system idle iowait irq softirq steal guest ..."
-- (the guest columns are in user already; the time that a hypervisor took
-- is busy time for this system, so it is in the total and not in the idle)
function model.cpu_linux(text)
    local line = (text or ""):match("^cpu%s+([^\n]*)")
    local n = numbers(line)
    if #n < 4 then
        return nil
    end
    local user = n[1] + n[2]
    local system = n[3] + (n[6] or 0) + (n[7] or 0)
    local idle = n[4] + (n[5] or 0)
    return {user = user, system = system, idle = idle, total = user + system + idle + (n[8] or 0)}
end

-- `sysctl -n kern.cp_time` of the BSD systems: user, nice, system, [spin,]
-- interrupt, idle. FreeBSD and OpenBSD print the numbers, NetBSD labels them
-- ("user = 1, nice = 2, ..."); the idle time is the last one on all of them.
function model.cpu_bsd(text)
    local n = numbers(text)
    if #n < 5 then
        return nil
    end
    local sample = {user = n[1] + n[2], system = 0, idle = n[#n], total = 0}
    for i = 3, #n - 1 do
        sample.system = sample.system + n[i]
    end
    sample.total = sample.user + sample.system + sample.idle
    return sample
end

-- The load between two samples: percentages of the time that the
-- processors (all of them together) were busy with the users' programs, with
-- the system, and idle. nil when the counters did not move, or went back.
function model.cpu_load(before, after)
    if not before or not after then
        return nil
    end
    local total = after.total - before.total
    if total <= 0 or after.idle < before.idle or after.user < before.user or after.system < before.system then
        return nil
    end
    local function share(name)
        return clamp(100 * (after[name] - before[name]) / total, 0, 100)
    end
    local idle = share("idle")
    return {busy = 100 - idle, user = share("user"), system = share("system"), idle = idle}
end

-- Memory and swap ---------------------------------------------------------

-- /proc/meminfo: what the programs can still get is MemAvailable (the cache
-- is not "used"); the kernels before 3.14 have no such line.
-- Returns memory, swap (nil when there is no swap).
function model.meminfo(text)
    local kb = {}
    for key, value in (text or ""):gmatch("(%w+):%s*(%d+)") do
        kb[key] = tonumber(value)
    end
    if not kb.MemTotal then
        return nil, nil
    end
    local available = kb.MemAvailable or ((kb.MemFree or 0) + (kb.Buffers or 0) + (kb.Cached or 0))
    local memory = {total = kb.MemTotal * KIB, used = clamp(kb.MemTotal - available, 0, kb.MemTotal) * KIB}
    local swap
    if kb.SwapTotal and kb.SwapTotal > 0 then
        swap = {total = kb.SwapTotal * KIB, used = clamp(kb.SwapTotal - (kb.SwapFree or 0), 0, kb.SwapTotal) * KIB}
    end
    return memory, swap
end

-- FreeBSD: the size of a page, the number of pages and of the free and the
-- inactive ones (the inactive pages are given back when somebody asks)
function model.memory_freebsd(text)
    local n = numbers(text)
    if #n < 4 or n[2] <= 0 then
        return nil
    end
    local size, pages, free, inactive = n[1], n[2], n[3], n[4]
    return {total = pages * size, used = clamp(pages - free - inactive, 0, pages) * size}
end

-- OpenBSD: the memory of the machine, then the line of top(1):
-- "Memory: Real: 78M/1288M act/tot Free: 664M Cache: 347M Swap: 0K/2043M"
-- The cache is given back like the free memory.
function model.memory_openbsd(text)
    local total = model.parse_size((text or ""):match("^%s*(%d+)"))
    local free = model.parse_size((text or ""):match("Free:%s*(%S+)"))
    local cache = model.parse_size((text or ""):match("Cache:%s*(%S+)"))
    if not total or total <= 0 or not free then
        return nil
    end
    return {total = total, used = clamp(total - free - (cache or 0), 0, total)}
end

-- NetBSD: the memory of the machine, then the line of top(1):
-- "Memory: 1124M Act, 558M Inact, 6904K Wired, 41M Exec, 1604M File, 20M Free"
-- The pages of files are in Act and Inact too, and they are given back.
function model.memory_netbsd(text)
    local total = model.parse_size((text or ""):match("^%s*(%d+)"))
    local line = (text or ""):match("Memory:[^\n]*") or ""
    local active = model.parse_size(line:match("([%d%.]+%a?) Act"))
    local inactive = model.parse_size(line:match("([%d%.]+%a?) Inact"))
    local wired = model.parse_size(line:match("([%d%.]+%a?) Wired"))
    local files = model.parse_size(line:match("([%d%.]+%a?) File")) or 0
    if not total or total <= 0 or not (active and inactive and wired) then
        return nil
    end
    return {total = total, used = clamp(active + inactive + wired - files, 0, total)}
end

-- FreeBSD `swapinfo -k`: a line for each device (and "Total" for several)
function model.swap_swapinfo(text)
    local total, used = 0, 0
    for line in (text or ""):gmatch("[^\n]+") do
        local name, size, taken = line:match("^(%S+)%s+(%d+)%s+(%d+)%s+%d+")
        if name and name ~= "Total" then
            total, used = total + tonumber(size), used + tonumber(taken)
        end
    end
    if total <= 0 then
        return nil
    end
    return {total = total * KIB, used = clamp(used, 0, total) * KIB}
end

-- OpenBSD and NetBSD `swapctl -sk`:
-- "total: 2092467 1K-blocks allocated, 0 used, 2092467 available"
-- "total: 8388608 KBytes allocated, 0 KBytes used, 8388608 KBytes available"
function model.swap_swapctl(text)
    local size, taken = (text or ""):match("total:%s+(%d+)%s+%S+%s+allocated,%s+(%d+)")
    size, taken = tonumber(size), tonumber(taken)
    if not size or size <= 0 then
        return nil
    end
    return {total = size * KIB, used = clamp(taken or 0, 0, size) * KIB}
end

-- Network -----------------------------------------------------------------

-- The adapters that have no traffic of their own: the loopback, and the
-- virtual ones that carry what a real adapter carries (a bridge, a tunnel, a
-- VLAN), so counting them would count the bytes twice.
local virtual = {
    "lo", "pflog", "pfsync", "enc", "bridge", "tun", "tap", "vether", "vlan", "gif", "gre", "epair",
    "wg", "veth", "ipsec", "vxlan", "carp", "lagg", "trunk", "stf", "ppp", "sl", "faith", "ipfw",
    "virbr", "vnet", "docker", "br-", "vboxnet",
}

function model.is_virtual(name)
    name = tostring(name or ""):gsub("%*$", "")
    for _, prefix in ipairs(virtual) do
        if name:sub(1, #prefix) == prefix then
            local rest = name:sub(#prefix + 1)
            -- "lo0" and "tun3" are meant; "log" or "bridgeport" are not
            if rest == "" or rest:match("^[%d%.%-_:]") or prefix:sub(-1) == "-" then
                return true
            end
        end
    end
    return false
end

-- The counters of one adapter go to the total and to the list of adapters
local function add_adapter(net, name, rx, tx)
    net.rx, net.tx = net.rx + rx, net.tx + tx
    net.adapters[name] = {rx = rx, tx = tx}
end

-- /proc/net/dev; `accept(name)` says which adapters count. nil without a
-- table of counters. Also returns the names that were counted.
function model.net_linux(text, accept)
    local net, names = {rx = 0, tx = 0, adapters = {}}, {}
    for line in (text or ""):gmatch("[^\n]+") do
        local name, rest = line:match("^%s*([^:|]+):(.*)$")
        local n = name and numbers(rest) or {}
        if #n >= 9 and not model.is_virtual(name) and (not accept or accept(name)) then
            add_adapter(net, name, n[1], n[9])
            names[#names + 1] = name
        end
    end
    if #names == 0 then
        return nil, names
    end
    return net, names
end

-- `netstat -ibn` of the BSD systems; the line with <Link> of each adapter
-- has the counters of the whole adapter (the lines of its addresses repeat
-- them). FreeBSD prints the packets, the errors and the collisions too, and
-- an adapter without an address has no Address column, so the counters are
-- found from the end of the line:
--   FreeBSD:  ... Ibytes Opkts Oerrs Obytes Coll
--   OpenBSD, NetBSD:  ... Ibytes Obytes
function model.net_bsd(text, platform)
    local net, names = {rx = 0, tx = 0, adapters = {}}, {}
    for line in (text or ""):gmatch("[^\n]+") do
        local fields = {}
        for field in line:gmatch("%S+") do
            fields[#fields + 1] = field
        end
        local name = fields[1]
        if name and fields[3] and fields[3]:find("^<Link") and not model.is_virtual(name) then
            local at = #fields
            local received, sent
            if platform == "freebsd" then
                received, sent = tonumber(fields[at - 4]), tonumber(fields[at - 1])
            else
                received, sent = tonumber(fields[at - 1]), tonumber(fields[at])
            end
            if received and sent then
                name = name:gsub("%*$", "")
                add_adapter(net, name, received, sent)
                names[#names + 1] = name
            end
        end
    end
    if #names == 0 then
        return nil, names
    end
    return net, names
end

-- How many bytes came and went between two samples of the network. Only the
-- adapters that are in both samples count: a card that was plugged in with
-- gigabytes behind it is not a burst of traffic, and one that was unplugged
-- does not make the traffic negative. An adapter whose counter went back (it
-- was reset) counts from zero. nil without both samples.
function model.net_delta(before, after)
    if not before or not after then
        return nil
    end
    local delta = {rx = 0, tx = 0}
    for name, now in pairs(after.adapters) do
        local then_ = before.adapters[name]
        if then_ then
            delta.rx = delta.rx + (now.rx >= then_.rx and now.rx - then_.rx or now.rx)
            delta.tx = delta.tx + (now.tx >= then_.tx and now.tx - then_.tx or now.tx)
        end
    end
    return delta
end

-- The speed in bytes per second between two counters read `seconds` apart;
-- nil when the counter went back (the adapter was reset) or there is no time
function model.rate(before, after, seconds)
    if not before or not after or not seconds or seconds <= 0 or after < before then
        return nil
    end
    return (after - before) / seconds
end

-- The speed as a share of a link of `mbits` megabits per second
function model.link_share(bytes_per_second, mbits)
    if not bytes_per_second or not mbits or mbits <= 0 then
        return nil
    end
    return clamp(100 * bytes_per_second * 8 / (mbits * 1e6), 0, 100)
end

-- Text ----------------------------------------------------------------------

function model.percent_text(value)
    if not value then
        return "--"
    end
    return string.format("%d%%", math.floor(clamp(value, 0, 100) + 0.5))
end

-- The units of bytes, for the panel (short) and for the window
local units = {"B", "K", "M", "G", "T"}

local function scaled(value)
    local power = 0
    while value >= 999.5 and power < 4 do
        value, power = value / KIB, power + 1
    end
    return value, power
end

-- A speed that fits in four characters: "0", "512B", "12K", "1.2M"
function model.rate_text(bytes_per_second)
    if not bytes_per_second then
        return "--"
    end
    if bytes_per_second < 0.5 then
        return "0"
    end
    local value, power = scaled(bytes_per_second)
    if power > 0 and value < 9.95 then
        return string.format("%.1f%s", value, units[power + 1])
    end
    return string.format("%d%s", math.floor(value + 0.5), units[power + 1])
end

-- The same for the window: "1.2 MiB/s"
function model.rate_long(bytes_per_second)
    if not bytes_per_second then
        return "--"
    end
    return model.bytes_text(bytes_per_second) .. "/s"
end

-- An amount of bytes: "0 B", "12 KiB", "3.4 GiB"
function model.bytes_text(bytes)
    if not bytes then
        return "--"
    end
    local value, power = scaled(math.max(bytes, 0))
    local unit = power == 0 and "B" or units[power + 1] .. "iB"
    if power > 0 and value < 9.95 then
        return string.format("%.1f %s", value, unit)
    end
    return string.format("%d %s", math.floor(value + 0.5), unit)
end

-- A share of memory in percent, or nil without a total
function model.share(part)
    if not part or part.total <= 0 then
        return nil
    end
    return 100 * part.used / part.total
end

return model
