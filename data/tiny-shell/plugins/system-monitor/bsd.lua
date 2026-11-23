-- FreeBSD, OpenBSD and NetBSD: the numbers come from sysctl, top, swapctl and
-- netstat. One short shell script runs them all, so a sample is one program
-- and not five; every part starts with a line "@name" and a part that fails
-- is only missing in the sample.

local model = require("model")

local bsd = {}

-- sbin is not in PATH of an ordinary user of NetBSD
local PATH = "PATH=$PATH:/sbin:/usr/sbin:/usr/local/sbin\n"

bsd.scripts = {
    freebsd = PATH
        .. "echo @cpu; sysctl -n kern.cp_time\n"
        .. "echo @memory; sysctl -n hw.pagesize vm.stats.vm.v_page_count"
        .. " vm.stats.vm.v_free_count vm.stats.vm.v_inactive_count\n"
        .. "echo @swap; swapinfo -k\n"
        .. "echo @net; netstat -ibn\n",
    openbsd = PATH
        .. "echo @cpu; sysctl -n kern.cp_time\n"
        .. "echo @memory; sysctl -n hw.physmem; top -b -d1 | grep '^Memory'\n"
        .. "echo @swap; swapctl -sk\n"
        .. "echo @net; netstat -ibn\n",
    netbsd = PATH
        .. "echo @cpu; sysctl -n kern.cp_time\n"
        .. "echo @memory; vmstat -s | grep -E 'bytes per page|pages managed|pages free$|cached file pages'\n"
        .. "echo @swap; swapctl -sk\n"
        .. "echo @net; netstat -ibn\n",
}

-- The parts of the output of the script: {cpu = "...", memory = "..."}
function bsd.split(text)
    local parts, name = {}, nil
    for line in (text or ""):gmatch("[^\n]*\n?") do
        local mark = line:match("^@(%a+)%s*$")
        if mark then
            name, parts[mark] = mark, ""
        elseif name then
            parts[name] = parts[name] .. line
        end
    end
    return parts
end

-- The sample that the parts give
function bsd.sample(parts, platform)
    local sample = {cpu = model.cpu_bsd(parts.cpu), net = model.net_bsd(parts.net, platform)}
    if platform == "freebsd" then
        sample.memory = model.memory_freebsd(parts.memory)
        sample.swap = model.swap_swapinfo(parts.swap)
    else
        local memory = platform == "openbsd" and model.memory_openbsd or model.memory_netbsd
        sample.memory = memory(parts.memory)
        sample.swap = model.swap_swapctl(parts.swap)
    end
    return sample
end

function bsd.read(done, platform)
    platform = platform or tiny.platform
    tiny.exec({"sh", "-c", bsd.scripts[platform]}, function(result)
        if result.status < 0 or result.stdout == "" then
            return done(nil)
        end
        done(bsd.sample(bsd.split(result.stdout), platform))
    end)
end

return bsd
