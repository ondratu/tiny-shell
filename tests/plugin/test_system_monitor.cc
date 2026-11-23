#include "host_fixture.h"

#include <filesystem>
#include <fstream>

#ifdef TINY_HAVE_LUA

// The system monitor is Lua. The tests that look at the model and the readers
// copy its files into a plugin of their own, replace plugin.lua by a script
// and wait for the script to say that it is done. The files of
// tests/plugin/system-monitor/ are the output of real machines (Linux,
// FreeBSD, OpenBSD, NetBSD); the programs of the BSD systems are replaced by
// scripts that print them, so any system runs the tests.

namespace {

namespace fs = std::filesystem;

const char* const prelude = R"LUA(
local function eq(got, want, what)
    if got ~= want then
        tiny.log("FAIL " .. what .. ": got " .. tostring(got) .. ", want " .. tostring(want))
    end
end
local function near(got, want, what)
    if type(got) ~= "number" or math.abs(got - want) > 0.01 then
        tiny.log("FAIL " .. what .. ": got " .. tostring(got) .. ", want " .. tostring(want))
    end
end
local function yes(value, what)
    if not value then
        tiny.log("FAIL " .. what)
    end
end
local function fixture(name)
    return assert(tiny.read_file(tiny.plugin.dir .. "/fixtures/" .. name))
end
local function done()
    tiny.log("DONE")
end
local MiB = 1048576
)LUA";

class SystemMonitorTest : public HostTest {
  protected:
    void SetUp() override
    {
        source = "data/tiny-shell/plugins/system-monitor";
        if (!fs::exists(source)) {
            GTEST_SKIP() << "run from the directory of the sources";
        }
    }

    //! Copies the plugin into the tree, with the fixtures; `lua` replaces
    //! plugin.lua when it is not empty
    void install(const std::string& lua)
    {
        const fs::path dir = tree.path / "monitor";
        std::string ini;
        std::getline(std::ifstream(source / "plugin.ini"), ini, '\0');
        ini.replace(ini.find("enabled = no"), 12, "enabled = yes");
        tree.add("monitor", lua.empty() ? "" : std::string(prelude) + lua, ini);
        for (const auto& entry : fs::directory_iterator(source)) {
            const std::string name = entry.path().filename().string();
            if (name != "plugin.ini" && (lua.empty() || name != "plugin.lua")) {
                fs::copy_file(entry.path(), dir / name,
                              fs::copy_options::overwrite_existing);
            }
        }
        fs::create_directories(dir / "fixtures");
        for (const auto& entry :
             fs::directory_iterator("tests/plugin/system-monitor")) {
            fs::copy_file(entry.path(),
                          dir / "fixtures" / entry.path().filename(),
                          fs::copy_options::overwrite_existing);
        }
    }

    //! Runs the script in the plugin; true when it said DONE and nothing FAIL
    bool run(const std::string& body)
    {
        install(body);
        PluginHost host(services);
        host.apply(tree.find(), config);
        const bool finished =
                loop.run_until([&] { return logged("plugin monitor: DONE"); });
        for (const std::string& line : lines) {
            if (line.find("FAIL") != std::string::npos) {
                ADD_FAILURE() << line;
            }
        }
        EXPECT_EQ(host.error_of("monitor"), "");
        EXPECT_TRUE(finished) << "the script did not finish";
        host.stop();
        return finished;
    }

    //! Programs of the BSD systems that print the files of the fixtures of
    //! the system that FAKE_BSD names
    void make_programs(const fs::path& bin)
    {
        fs::create_directories(bin);
        const std::string fixtures =
                fs::absolute("tests/plugin/system-monitor").string();
        const auto write = [&](const std::string& name,
                               const std::string& body) {
            const fs::path path = bin / name;
            std::ofstream(path)
                    << "#!/bin/sh\nF=" << fixtures << "\nP=$FAKE_BSD\n"
                    << body;
            fs::permissions(path, fs::perms::owner_all);
        };
        write("sysctl", "case \"$*\" in\n"
                        "  *cp_time*) cat \"$F/$P-cp_time.txt\" ;;\n"
                        "  *pagesize*) cat \"$F/freebsd-memory.txt\" ;;\n"
                        "  *physmem*) head -n 1 \"$F/$P-memory.txt\" ;;\n"
                        "esac\n");
        write("top",
              "tail -n +2 \"$F/$P-memory.txt\"\n"
              "echo\necho '  PID USERNAME PRI NICE  SIZE   RES STATE   TIME "
              "CPU COMMAND'\n");
        write("swapinfo", "cat \"$F/freebsd-swapinfo.txt\"\n");
        write("swapctl", "cat \"$F/$P-swapctl.txt\"\n");
        write("netstat", "cat \"$F/$P-netstat.txt\"\n");
    }

    fs::path source;
};

} // namespace

TEST_F(SystemMonitorTest, sizes_and_texts_are_short_and_round_properly)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
eq(model.parse_size("1124M"), 1124 * MiB, "megabytes")
eq(model.parse_size("6904K"), 6904 * 1024, "kilobytes")
eq(model.parse_size("1.5G"), 1.5 * 1024 * MiB, "a fraction")
eq(model.parse_size("512"), 512, "bytes")
eq(model.parse_size("lots"), nil, "not a size")
eq(model.parse_size(nil), nil, "nothing")
eq(model.percent_text(nil), "--", "unknown")
eq(model.percent_text(0), "0%", "0")
eq(model.percent_text(33.4), "33%", "rounded down")
eq(model.percent_text(33.5), "34%", "rounded up")
eq(model.percent_text(140), "100%", "never above 100")
eq(model.percent_text(-3), "0%", "never below 0")
eq(model.rate_text(nil), "--", "no speed")
eq(model.rate_text(0), "0", "nothing moves")
eq(model.rate_text(0.2), "0", "less than a byte")
eq(model.rate_text(512), "512B", "bytes")
eq(model.rate_text(1536), "1.5K", "one decimal under 10")
eq(model.rate_text(50 * 1024), "50K", "whole above")
eq(model.rate_text(1000), "1.0K", "1000 is already a kilobyte")
eq(model.rate_text(1.2 * MiB), "1.2M", "megabytes")
eq(model.rate_text(300 * MiB), "300M", "three digits")
eq(model.rate_text(2.5 * 1024 * MiB), "2.5G", "gigabytes")
yes(#model.rate_text(1023 * MiB) <= 4, "fits the panel")
eq(model.rate_long(2 * MiB), "2.0 MiB/s", "for the window")
eq(model.rate_long(nil), "--", "no speed in the window")
eq(model.bytes_text(nil), "--", "no amount")
eq(model.bytes_text(0), "0 B", "zero")
eq(model.bytes_text(-5), "0 B", "never negative")
eq(model.bytes_text(1536), "1.5 KiB", "KiB")
eq(model.bytes_text(3.4 * 1024 * MiB), "3.4 GiB", "GiB")
eq(model.bytes_text(20 * 1024 * MiB), "20 GiB", "no decimal above ten")
eq(model.share(nil), nil, "no memory")
eq(model.share({total = 0, used = 0}), nil, "no total")
near(model.share({total = 200, used = 50}), 25, "a quarter")
near(model.link_share(125000, 1), 100, "1 Mbit/s full")
near(model.link_share(12500000, 1000), 10, "a tenth of a gigabit")
near(model.link_share(1e12, 100), 100, "never above 100")
eq(model.link_share(nil, 100), nil, "no speed")
eq(model.link_share(1000, 0), nil, "no link")
done()
)LUA"));
}

TEST_F(SystemMonitorTest,
       the_load_is_the_share_of_the_time_that_the_cpus_worked)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
-- Linux: iowait is idle, the time taken by a hypervisor is not
local linux = model.cpu_linux(fixture("linux-stat.txt"))
eq(linux.user, 12475550 + 28561, "user and nice")
eq(linux.system, 2796347 + 62550, "system, irq and softirq")
eq(linux.idle, 477972751 + 257067, "idle and iowait")
eq(linux.total, linux.user + linux.system + linux.idle, "total")
eq(model.cpu_linux("cpu 1 2 3"), nil, "too short")
eq(model.cpu_linux(""), nil, "empty")
eq(model.cpu_linux(nil), nil, "nothing")
local steal = model.cpu_linux("cpu  10 0 10 70 0 0 0 10 0 0\n")
eq(steal.total, 100, "steal counts in the total")
near(model.cpu_load({user = 0, system = 0, idle = 0, total = 0}, steal).busy, 30, "and is not idle")

-- the BSD systems: numbers, commas or labels; the idle time is the last
local free = model.cpu_bsd(fixture("freebsd-cp_time.txt"))
eq(free.user, 63790 + 1, "FreeBSD user and nice")
eq(free.system, 14261 + 873, "FreeBSD system and interrupts")
eq(free.idle, 21625723, "FreeBSD idle")
local open = model.cpu_bsd(fixture("openbsd-cp_time.txt"))
eq(open.user, 98382, "OpenBSD user")
eq(open.system, 16097 + 344 + 3624, "OpenBSD system, spin and interrupts")
eq(open.idle, 9637807, "OpenBSD idle")
local net = model.cpu_bsd(fixture("netbsd-cp_time.txt"))
eq(net.user, 33498 + 96, "NetBSD user and nice")
eq(net.system, 5335 + 531, "NetBSD system and interrupts")
eq(net.idle, 17054477, "NetBSD idle")
eq(model.cpu_bsd("1 2 3"), nil, "too short")
eq(model.cpu_bsd(nil), nil, "nothing")

-- the difference of two samples
local a = {user = 100, system = 50, idle = 850, total = 1000}
local b = {user = 130, system = 60, idle = 910, total = 1100}
local load = model.cpu_load(a, b)
near(load.busy, 40, "busy")
near(load.user, 30, "user")
near(load.system, 10, "system")
near(load.idle, 60, "idle")
eq(model.cpu_load(a, a), nil, "no time passed")
eq(model.cpu_load(b, a), nil, "counters went back")
eq(model.cpu_load(nil, a), nil, "no first sample")
eq(model.cpu_load(a, nil), nil, "no second sample")
done()
)LUA"));
}

TEST_F(SystemMonitorTest, memory_and_swap_leave_out_what_the_system_gives_back)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
local memory, swap = model.meminfo(fixture("linux-meminfo.txt"))
eq(memory.total, 32275216 * 1024, "Linux total")
eq(memory.used, (32275216 - 10560620) * 1024, "the used memory is the total less MemAvailable")
eq(swap.total, 33148924 * 1024, "Linux swap")
eq(swap.used, (33148924 - 27498824) * 1024, "swap in use")
-- an old kernel has no MemAvailable: the free memory and the cache
memory, swap = model.meminfo("MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 50 kB\nCached: 250 kB\nSwapTotal: 0 kB\nSwapFree: 0 kB\n")
eq(memory.used, 600 * 1024, "without MemAvailable")
eq(swap, nil, "no swap")
eq(model.meminfo("nonsense"), nil, "not meminfo")
eq(model.meminfo(nil), nil, "nothing")

local free = model.memory_freebsd(fixture("freebsd-memory.txt"))
eq(free.total, 502060 * 4096, "FreeBSD total")
eq(free.used, (502060 - 201570 - 71073) * 4096, "the free and the inactive pages are not used")
eq(model.memory_freebsd("4096 0 0 0"), nil, "no pages")
eq(model.memory_freebsd(nil), nil, "nothing")

local open = model.memory_openbsd(fixture("openbsd-memory.txt"))
eq(open.total, 2130575360, "OpenBSD total")
eq(open.used, 2130575360 - 664 * MiB - 347 * MiB, "free and cache are not used")
eq(model.memory_openbsd("2130575360\nMemory: Real: 1M/2M act/tot"), nil, "no free memory")
eq(model.memory_openbsd(nil), nil, "nothing")

local net = model.memory_netbsd(fixture("netbsd-memory.txt"))
eq(net.total, 2146955264, "NetBSD total")
eq(net.used, (1124 + 558 - 1604) * MiB + 6904 * 1024, "the pages of files are not used")
eq(model.memory_netbsd("2146955264\nMemory: 1M Act, 1M Inact, 1M Wired, 1M Exec, 9M File, 1M Free").used, 0,
   "never below zero")
eq(model.memory_netbsd("1\nMemory: nonsense"), nil, "not top")
eq(model.memory_netbsd(nil), nil, "nothing")

local info = model.swap_swapinfo(fixture("freebsd-swapinfo.txt"))
eq(info.total, 2097152 * 1024, "swapinfo")
eq(info.used, 0, "swapinfo used")
local two = model.swap_swapinfo("Device 1K-blocks Used Avail Capacity\n/dev/a 100 10 90 10%\n/dev/b 300 30 270 10%\nTotal 400 40 360 10%\n")
eq(two.total, 400 * 1024, "devices are added up, the total line is not added again")
eq(two.used, 40 * 1024, "used")
eq(model.swap_swapinfo("Device 1K-blocks Used Avail Capacity\n"), nil, "no swap")
eq(model.swap_swapinfo(nil), nil, "nothing")

local ctl = model.swap_swapctl(fixture("openbsd-swapctl.txt"))
eq(ctl.total, 2092467 * 1024, "OpenBSD swapctl")
eq(model.swap_swapctl(fixture("netbsd-swapctl.txt")).total, 8388608 * 1024, "NetBSD swapctl")
eq(model.swap_swapctl("total: 100 1K-blocks allocated, 25 used, 75 available").used, 25 * 1024, "used")
eq(model.swap_swapctl("no swap devices configured"), nil, "none")
eq(model.swap_swapctl("total: 0 1K-blocks allocated, 0 used, 0 available"), nil, "empty")
eq(model.swap_swapctl(nil), nil, "nothing")
done()
)LUA"));
}

TEST_F(SystemMonitorTest,
       the_network_counts_the_cards_and_neither_the_loopback_nor_the_virtual)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
for _, name in ipairs({"lo", "lo0", "pflog0", "enc0*", "virbr0", "vnet12", "docker0", "br-1a2b3c", "veth9f", "tun0",
                       "tap1", "bridge0", "vlan7", "wg0", "gif0", "epair0a", "lagg0", "vboxnet0"}) do
    yes(model.is_virtual(name), name .. " is virtual")
end
for _, name in ipairs({"eth0", "enp3s0", "wlp0s20f3", "em0", "wm0", "iwn0", "ue0", "re0", "log0", "tunnelx", "lofi",
                       "bridgeport"}) do
    yes(not model.is_virtual(name), name .. " is a card")
end

-- Linux
local counted, names = model.net_linux(fixture("linux-net-dev.txt"))
eq(counted.rx, 34552019586, "downloaded")
eq(counted.tx, 3683313698, "uploaded")
eq(#names, 1, "one card")
eq(names[1], "wlp0s20f3", "the card")
counted = model.net_linux(fixture("linux-net-dev.txt"), function(name) return name ~= "wlp0s20f3" end)
eq(counted, nil, "the filter of the reader can leave a card out")
eq(model.net_linux("Inter-|   Receive\n face |bytes\n", nil), nil, "no adapters")
eq(model.net_linux(nil), nil, "nothing")
counted = model.net_linux("eth0: 10 1 0 0 0 0 0 0 20 1 0 0 0 0 0 0\neth1: 5 1 0 0 0 0 0 0 7 1 0 0 0 0 0 0\n")
eq(counted.rx, 15, "two cards: download")
eq(counted.tx, 27, "two cards: upload")

-- BSD: the counters of an adapter, from the line of the link
counted, names = model.net_bsd(fixture("freebsd-netstat.txt"), "freebsd")
eq(counted.rx, 9826867, "FreeBSD download")
eq(counted.tx, 389144, "FreeBSD upload")
eq(#names, 1, "FreeBSD counts em0 once and not lo0")
counted = model.net_bsd(fixture("openbsd-netstat.txt"), "openbsd")
eq(counted.rx, 49164599, "OpenBSD download")
eq(counted.tx, 3330567, "OpenBSD upload")
counted = model.net_bsd(fixture("netbsd-netstat.txt"), "netbsd")
eq(counted.rx, 8442380, "NetBSD download")
eq(counted.tx, 437854, "NetBSD upload")
-- FreeBSD: an adapter without an address has no Address column
counted = model.net_bsd("Name Mtu Network Address Ipkts Ierrs Idrop Ibytes Opkts Oerrs Obytes Coll\n"
    .. "ue0 1500 <Link#3> 10 0 0 111 5 0 222 0\n"
    .. "em0 1500 <Link#1> 52:54:00:24:68:74 10 0 0 1000 5 0 2000 0\n", "freebsd")
eq(counted.rx, 1111, "FreeBSD without an address: download")
eq(counted.tx, 2222, "FreeBSD without an address: upload")
eq(model.net_bsd("Name Mtu Network Address Ibytes Obytes\nlo0 32768 <Link> 96 96\n", "openbsd"), nil, "only the loopback")
eq(model.net_bsd(nil, "freebsd"), nil, "nothing")

-- the speed
near(model.rate(1000, 5000, 2), 2000, "bytes per second")
eq(model.rate(5000, 1000, 2), nil, "a counter that went back (the adapter was reset)")
eq(model.rate(1000, 5000, 0), nil, "no time")
eq(model.rate(nil, 5000, 2), nil, "no first sample")
done()
)LUA"));
}

TEST_F(SystemMonitorTest, the_linux_reader_reads_proc_and_sys)
{
    const fs::path root = tree.path / "fake";
    fs::create_directories(root / "proc" / "net");
    fs::create_directories(root / "sys" / "wlp0s20f3" / "device");
    fs::create_directories(root / "sys" / "tunnel0");
    const auto copy = [&](const std::string& from, const std::string& to) {
        fs::copy_file("tests/plugin/system-monitor/" + from, root / to);
    };
    copy("linux-stat.txt", "proc/stat");
    copy("linux-meminfo.txt", "proc/meminfo");
    copy("linux-net-dev.txt", "proc/net/dev");
    EXPECT_TRUE(run("local ROOT = '" + root.string() + "'\n" + R"LUA(
local linux = require("linux")
linux.proc = ROOT .. "/proc/"
linux.net = ROOT .. "/sys/"
yes(linux.is_card("wlp0s20f3"), "a card has a device")
yes(not linux.is_card("tunnel0"), "a tunnel has none")
yes(not linux.is_card("missing"), "an adapter that is not there")
linux.read(function(sample)
    eq(sample.cpu.idle, 477972751 + 257067, "processor")
    eq(sample.memory.total, 32275216 * 1024, "memory")
    eq(sample.swap.total, 33148924 * 1024, "swap")
    eq(sample.net.rx, 34552019586, "download")
    -- a computer without swap or adapters: the parts are missing
    linux.proc = ROOT .. "/nothing/"
    linux.read(function(empty)
        eq(empty.cpu, nil, "no processor")
        eq(empty.memory, nil, "no memory")
        eq(empty.swap, nil, "no swap")
        eq(empty.net, nil, "no network")
        done()
    end)
end)
)LUA"));
}

TEST_F(SystemMonitorTest, the_bsd_reader_runs_one_script_and_splits_its_output)
{
    const fs::path bin = tree.path / "fake" / "bin";
    make_programs(bin);
    const EnvVar path("PATH", bin.string() + ":" + getenv("PATH"));
    EXPECT_TRUE(run(R"LUA(
local bsd = require("bsd")
local parts = bsd.split("@cpu\n1 2 3\n@memory\na\nb\n@swap\n@net\nc")
eq(parts.cpu, "1 2 3\n", "a part")
eq(parts.memory, "a\nb\n", "two lines")
eq(parts.swap, "", "an empty part")
eq(parts.net, "c", "the last line without a newline")
eq(next(bsd.split("")), nil, "nothing")
eq(next(bsd.split(nil)), nil, "nil")
eq(next(bsd.split("junk before\n")), nil, "text before the first mark is dropped")
done()
)LUA"));

    // every system: its fixtures, through the programs that print them
    struct Expected {
        const char* system;
        long memory;
        long swap_kb;
        long download;
    };
    const Expected systems[] = {
            {"freebsd", 502060L * 4096, 2097152, 9826867},
            {"openbsd", 2130575360L, 2092467, 49164599},
            {"netbsd", 2146955264L, 8388608, 8442380},
    };
    for (const Expected& want : systems) {
        const EnvVar system("FAKE_BSD", want.system);
        lines.clear();
        EXPECT_TRUE(run("local SYSTEM = '" + std::string(want.system) +
                        "'\nlocal MEMORY, SWAP, DOWNLOAD = " +
                        std::to_string(want.memory) + ", " +
                        std::to_string(want.swap_kb) + ", " +
                        std::to_string(want.download) + "\n" + R"LUA(
local bsd = require("bsd")
bsd.read(function(sample)
    yes(sample, SYSTEM .. ": a sample")
    eq(sample.memory.total, MEMORY, SYSTEM .. ": memory")
    eq(sample.swap.total, SWAP * 1024, SYSTEM .. ": swap")
    eq(sample.net.rx, DOWNLOAD, SYSTEM .. ": download")
    yes(sample.cpu.total > sample.cpu.idle, SYSTEM .. ": processor")
    done()
end, SYSTEM)
)LUA"));
    }
}

TEST_F(SystemMonitorTest,
       a_failing_script_gives_no_sample_and_a_part_that_fails_is_missing)
{
    const fs::path bin = tree.path / "fake" / "bin";
    fs::create_directories(bin);
    const auto write = [&](const std::string& name, const std::string& body) {
        std::ofstream(bin / name) << "#!/bin/sh\n" << body;
        fs::permissions(bin / name, fs::perms::owner_all);
    };
    write("sysctl", "echo '1 2 3 4 100'\n");
    write("swapctl", "echo 'no swap devices configured'\n");
    write("top", "exit 1\n");
    write("netstat", "exit 1\n");
    const EnvVar path("PATH", bin.string() + ":" + getenv("PATH"));
    EXPECT_TRUE(run(R"LUA(
local bsd = require("bsd")
bsd.read(function(sample)
    yes(sample, "a sample")
    eq(sample.cpu.idle, 100, "the part that works")
    eq(sample.memory, nil, "no memory")
    eq(sample.swap, nil, "no swap")
    eq(sample.net, nil, "no network")
    -- a system that does not start sh gives nothing at all
    bsd.scripts.nosuch = "exit 3\n"
    bsd.read(function(none)
        eq(none, nil, "no output, no sample")
        done()
    end, "nosuch")
end, "netbsd")
)LUA"));
}

#endif
