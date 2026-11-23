-- Linux: everything is in /proc and /sys, so a sample is read at once.

local model = require("model")

local linux = {
    -- where the system keeps the numbers (a test points them elsewhere)
    proc = "/proc/",
    net = "/sys/class/net/",
}

-- <net>/<name>/device is there for a card and not for a bridge, a tunnel or a
-- virtual pair: only the cards carry traffic of their own
function linux.is_card(name)
    return tiny.list_dir(linux.net .. name .. "/device") ~= nil
end

function linux.read(done)
    local sample = {cpu = model.cpu_linux(tiny.read_file(linux.proc .. "stat"))}
    sample.memory, sample.swap = model.meminfo(tiny.read_file(linux.proc .. "meminfo"))
    sample.net = model.net_linux(tiny.read_file(linux.proc .. "net/dev"), linux.is_card)
    done(sample)
end

return linux
