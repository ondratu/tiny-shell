#include "host_fixture.h"

#include <filesystem>
#include <fstream>

#ifdef TINY_HAVE_LUA

// The network plugin is Lua. Each test copies its files into a plugin of its
// own, replaces plugin.lua by a script that looks at them (the model, the
// parsers of the backends, the icons and the window that a state gives) and
// waits for the script to say that it is done. The programs of the system
// (nmcli, ip, iw) are replaced by scripts that print the files of
// tests/plugin/network/ and write down how they were called.

namespace {

namespace fs = std::filesystem;

const char* const prelude = R"LUA(
local function eq(got, want, what)
    if got ~= want then
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
local function list(...)
    return table.concat({...}, ",")
end
)LUA";

class NetworkTest : public HostTest {
  protected:
    void SetUp() override
    {
        source = "data/tiny-shell/plugins/network";
        if (!fs::exists(source)) {
            GTEST_SKIP() << "run from the directory of the sources";
        }
    }

    //! Runs the script in the plugin; true when it said DONE and nothing FAIL
    bool run(const std::string& body)
    {
        const fs::path dir = tree.path / "net";
        tree.add("net", std::string(prelude) + body);
        for (const auto& entry : fs::directory_iterator(source)) {
            if (entry.path().extension() == ".lua" &&
                entry.path().filename() != "plugin.lua") {
                fs::copy_file(entry.path(), dir / entry.path().filename(),
                              fs::copy_options::overwrite_existing);
            }
        }
        fs::create_directories(dir / "fixtures");
        for (const auto& entry :
             fs::directory_iterator("tests/plugin/network")) {
            fs::copy_file(entry.path(),
                          dir / "fixtures" / entry.path().filename(),
                          fs::copy_options::overwrite_existing);
        }
        PluginHost host(services);
        host.apply(tree.find(), config);
        const bool finished =
                loop.run_until([&] { return logged("plugin net: DONE"); });
        for (const std::string& line : lines) {
            if (line.find("FAIL") != std::string::npos) {
                ADD_FAILURE() << line;
            }
        }
        EXPECT_EQ(host.error_of("net"), "");
        EXPECT_TRUE(finished) << "the script did not finish";
        host.stop();
        return finished;
    }

    //! A directory like /sys/class/net with some adapters
    void make_sys(const fs::path& root)
    {
        const auto adapter = [&](const std::string& name, bool wireless,
                                 const std::string& flags,
                                 const std::string& carrier) {
            const fs::path dir = root / "net" / name;
            fs::create_directories(dir / "device");
            if (wireless) {
                fs::create_directories(dir / "wireless");
            }
            std::ofstream(dir / "type") << "1\n";
            std::ofstream(dir / "flags") << flags << "\n";
            if (!carrier.empty()) {
                std::ofstream(dir / "carrier") << carrier << "\n";
            }
            if (!wireless && carrier == "1") {
                std::ofstream(dir / "speed") << "1000\n";
            }
        };
        adapter("enp3s0", false, "0x1003", "1");
        adapter("down0", false, "0x1002", "");
        adapter("eth8", false, "0x1003", "1");
        adapter("eth9", false, "0x1003", "1");
        adapter("wlan0", true, "0x1003", "1");
        adapter("wlan1", true, "0x1003", "0");
        fs::create_directories(root / "net" / "virbr0"); // no device: a bridge
        std::ofstream(root / "net" / "virbr0" / "type") << "1\n";
        fs::create_directories(root / "net" / "lo");
        fs::copy_file("tests/plugin/network/proc-wireless.txt",
                      root / "wireless", fs::copy_options::overwrite_existing);
    }

    //! Programs that print the files of tests/plugin/network/ and note how
    //! they were called in `calls`
    void make_programs(const fs::path& bin)
    {
        fs::create_directories(bin);
        const fs::path fixtures = fs::absolute("tests/plugin/network");
        const auto write = [&](const std::string& name,
                               const std::string& body) {
            const fs::path path = bin / name;
            std::ofstream(path) << "#!/bin/sh\n" << body;
            fs::permissions(path, fs::perms::owner_all);
        };
        write("nmcli",
              "echo \"$*\" >> \"$FAKE_CALLS\"\n"
              "if [ -n \"$FAKE_STDIN\" ]; then echo \"stdin=$(cat)\" >> "
              "\"$FAKE_CALLS\"; fi\n"
              "F=" + fixtures.string() +
                      "\n"
                      "case \"$*\" in\n"
                      "  *permissions*) cat "
                      "\"$F/${FAKE_PERMISSIONS:-nmcli-permissions.txt}\" ;;\n"
                      "  *DEVICE,TYPE*) cat \"$F/nmcli-device.txt\" ;;\n"
                      "  *'wifi list'*) cat \"$F/nmcli-wifi.txt\" ;;\n"
                      "  *NAME,TYPE*) cat \"$F/nmcli-known.txt\" ;;\n"
                      "  *'radio wifi') echo \"${FAKE_RADIO:-enabled}\" ;;\n"
                      "  *) exit ${FAKE_STATUS:-0} ;;\n"
                      "esac\n");
        write("ip", "F=" + fixtures.string() +
                            "\n"
                            "case \"$*\" in\n"
                            "  *route*) cat \"$F/ip-route.json\" ;;\n"
                            "  *) cat \"$F/ip-addr.json\" ;;\n"
                            "esac\n");
        write("iw", "cat \"" + fixtures.string() + "/iw-link.txt\"\n");
    }

    fs::path source;
};

} // namespace

TEST_F(NetworkTest, the_model_knows_the_signal_the_bands_and_the_speed)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
eq(model.level_icon(nil), nil, "no signal")
eq(model.level_icon(100), "network-wireless-signal-excellent", "100")
eq(model.level_icon(80), "network-wireless-signal-excellent", "80")
eq(model.level_icon(79), "network-wireless-signal-good", "79")
eq(model.level_icon(55), "network-wireless-signal-good", "55")
eq(model.level_icon(54), "network-wireless-signal-ok", "54")
eq(model.level_icon(30), "network-wireless-signal-ok", "30")
eq(model.level_icon(29), "network-wireless-signal-weak", "29")
eq(model.level_icon(5), "network-wireless-signal-weak", "5")
eq(model.level_icon(4), nil, "4")
eq(model.level_icon(0), nil, "0")
eq(model.dbm_to_percent(-45), 100, "-45 dBm")
eq(model.dbm_to_percent(-75), 50, "-75 dBm")
eq(model.dbm_to_percent(-120), 0, "-120 dBm")
eq(model.dbm_to_percent(-30), 100, "-30 dBm")
eq(model.band(2437), "2.4 GHz", "2437")
eq(model.band(5180), "5 GHz", "5180")
eq(model.band(5955), "6 GHz", "5955")
eq(model.band(900), nil, "900")
eq(model.band(nil), nil, "no frequency")
eq(model.band_of_channel(6), "2.4 GHz", "channel 6")
eq(model.band_of_channel(36), "5 GHz", "channel 36")
eq(model.band_of_channel(0), nil, "channel 0")
eq(model.band_of_channel(nil), nil, "no channel")
eq(model.speed_text(1000), "1 Gb/s", "1000")
eq(model.speed_text(2500), "2.5 Gb/s", "2500")
eq(model.speed_text(100), "100 Mb/s", "100")
eq(model.speed_text(433.3), "433 Mb/s", "433.3")
eq(model.speed_text(0), nil, "0")
eq(model.speed_text(nil), nil, "no speed")
yes(model.is_secured("WPA2"), "WPA2 is secured")
yes(not model.is_secured("--"), "-- is open")
yes(not model.is_secured(""), "empty is open")
yes(not model.is_secured(nil), "nil is open")
eq(model.security_text("--"), "Open", "open")
eq(model.security_text("WPA2"), "WPA2", "wpa2")
eq(model.clean("a\nb\27c"), "a?b?c", "control characters of a name")
eq(model.clean(nil), nil, "no name")
done()
)LUA"));
}

TEST_F(NetworkTest, the_main_adapter_is_the_connected_one_with_the_best_route)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
eq(model.primary({}), nil, "none")
eq(model.primary({{name = "a", state = "disconnected"}}), nil, "not connected")
local wired = {name = "eth", state = "connected", metric = 100}
local wifi = {name = "wlan", state = "connected", metric = 600}
eq(model.primary({wifi, wired}).name, "eth", "the lower metric wins")
eq(model.primary({wired, wifi}).name, "eth", "in any order")
eq(model.primary({{name = "a", state = "connected"}, {name = "b", state = "connected"}}).name, "a",
   "without metrics the first one")
eq(model.primary({{name = "a", state = "connected"}, {name = "b", state = "connected", metric = 5}}).name, "b",
   "one with a route")
done()
)LUA"));
}

TEST_F(NetworkTest,
       the_networks_in_range_are_one_for_a_name_and_the_strongest_first)
{
    EXPECT_TRUE(run(R"LUA(
local model = require("model")
local nets = {
    {ssid = "A", signal = 40, known = false},
    {ssid = "A", signal = 70, known = true},
    {ssid = "B", signal = 90},
    {ssid = "B", signal = 10, known = true},
    {ssid = "", signal = 99},
    {ssid = "Mine", signal = 80},
    {ssid = "Z", signal = 40},
}
local out = model.list_networks(nets, "Mine", 10)
eq(#out, 3, "count")
eq(out[1].ssid, "B", "the strongest first")
eq(out[2].ssid, "A", "second")
eq(out[3].ssid, "Z", "third")
yes(out[1].known, "a weaker one that is saved makes the name saved")
yes(out[2].known, "the stronger one is saved")
eq(out[2].signal, 70, "A is the stronger one")
eq(#model.list_networks(nets, nil, 2), 2, "the limit")
eq(#model.list_networks(nil, nil, 2), 0, "nothing")
-- the same strength: by name
local same = model.list_networks({{ssid = "b", signal = 5}, {ssid = "a", signal = 5}}, nil, 5)
eq(same[1].ssid, "a", "by name")
done()
)LUA"));
}

TEST_F(NetworkTest, the_helpers_split_the_output_of_nmcli_and_pick_addresses)
{
    EXPECT_TRUE(run(R"LUA(
local util = require("util")
eq(list(table.unpack(util.split_terse("a:b:c"))), "a,b,c", "plain")
eq(list(table.unpack(util.split_terse("a\\:b:c"))), "a:b,c", "an escaped colon")
eq(list(table.unpack(util.split_terse("a\\\\:b"))), "a\\,b", "an escaped backslash")
eq(#util.split_terse(""), 1, "an empty line is one empty field")
eq(#util.split_terse("::"), 3, "empty fields")
eq(#util.split_terse("x\\"), 1, "a backslash at the end")
eq(#util.lines("a\nb\r\nc"), 3, "lines")
eq(util.trim("  x \n"), "x", "trim")
eq(util.trim(nil), nil, "trim nil")
eq(list(table.unpack(util.pick_addresses({"fe80::1/64", "2a01::1/64", "2a01::2/64", "2a01::3/64"}, 2))),
   "2a01::1/64,2a01::2/64", "global ones first, at most two")
eq(list(table.unpack(util.pick_addresses({"fe80::1/64"}, 3))), "fe80::1/64", "link-local when nothing else")
eq(#util.pick_addresses({"169.254.1.1/16"}, 3), 1, "ipv4 link-local when nothing else")
eq(#util.pick_addresses(nil, 3), 0, "nothing")
local seen = {}
util.each({1, 2, 3}, function(item, next) seen[#seen + 1] = item next() end, function()
    eq(list(table.unpack(seen)), "1,2,3", "each")
    done()
end)
)LUA"));
}

TEST_F(NetworkTest,
       the_kernel_and_ip_are_read_by_the_backend_without_networkmanager)
{
    const fs::path root = tree.path / "fake";
    make_sys(root);
    make_programs(root / "bin");
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    EXPECT_TRUE(run("local ROOT = '" + root.string() + "'\n" + R"LUA(
local sys = require("sys")
sys.root = ROOT .. "/net/"
sys.wireless_file = ROOT .. "/wireless"
local found = sys.devices()
local names = {}
for _, d in ipairs(found) do names[#names + 1] = d.name .. ":" .. d.kind end
eq(list(table.unpack(names)), "down0:ethernet,enp3s0:ethernet,eth8:ethernet,eth9:ethernet,wlan0:wifi,wlan1:wifi",
   "only the cards (no bridge, no loopback)")
sys.read(function(adapters)
    local by = {}
    for _, a in ipairs(adapters) do by[a.name] = a end
    eq(#adapters, 6, "adapters")
    eq(adapters[1].kind, "ethernet", "ethernet first")
    eq(adapters[#adapters].kind, "wifi", "wifi last")
    eq(by.enp3s0.state, "connected", "cable in")
    eq(by.enp3s0.speed, 1000, "speed")
    eq(by.enp3s0.ipv4[1], "192.168.1.23/24", "ipv4")
    eq(by.enp3s0.ipv6[1], "2a01:db8::7c1/64", "ipv6")
    eq(by.enp3s0.metric, 50, "the best default route")
    eq(by.down0.state, "disabled", "down")
    eq(by.wlan0.state, "connected", "wifi")
    eq(by.wlan0.wifi.ssid, "Home Net", "the name from iw")
    eq(by.wlan0.wifi.band, "5 GHz", "the band from iw")
    eq(by.wlan0.wifi.signal, 100, "the signal from iw")
    eq(by.wlan0.metric, 600, "wifi route")
    eq(by.wlan1.state, "disconnected", "up, not associated")
    eq(by.wlan1.wifi, nil, "no wifi details")
    yes(not by.enp3s0.can_toggle, "no changes without networkmanager")
    eq(by.enp3s0.reason, "Managed by the system", "reason")
    -- without iw the signal comes from /proc/net/wireless
    sys.read(function(again)
        local wlan0
        for _, a in ipairs(again) do if a.name == "wlan0" then wlan0 = a end end
        eq(wlan0.wifi.ssid, nil, "no name without iw")
        eq(wlan0.wifi.signal, 70, "the signal of /proc/net/wireless: 49 of 70")
        done()
    end, {iw = false})
end)
)LUA"));
}

TEST_F(NetworkTest, the_parsers_of_ip_iw_and_proc_read_what_the_programs_print)
{
    EXPECT_TRUE(run(R"LUA(
local sys = require("sys")
local addresses = sys.parse_addresses(fixture("ip-addr.json"))
eq(addresses.wlan0.ipv4[1], "10.0.0.15/24", "wlan0 ipv4")
eq(addresses.wlan0.ipv6[1], "fe80::9a/64", "wlan0 link-local")
eq(#addresses.down0.ipv4, 0, "no address")
eq(addresses.nothing, nil, "unknown")
eq(next(sys.parse_addresses("not json")), nil, "bad json")
eq(next(sys.parse_addresses("{}")), nil, "not a list")
eq(next(sys.parse_addresses(nil)), nil, "nil")
local routes = sys.parse_routes(fixture("ip-route.json"))
eq(routes.enp3s0, 50, "lowest metric")
eq(routes.wlan0, 600, "wifi metric")
eq(next(sys.parse_routes("[]")), nil, "no routes")
local quality = sys.parse_wireless(fixture("proc-wireless.txt"))
eq(quality.wlan0, 70, "49/70")
eq(quality.wlan1, 100, "70/70")
eq(next(sys.parse_wireless("")), nil, "empty")
local iw = sys.parse_iw(fixture("iw-link.txt"))
eq(iw.ssid, "Home Net", "ssid with a space")
eq(iw.band, "5 GHz", "band")
eq(iw.rate, 433.3, "rate")
eq(iw.signal, 100, "signal")
eq(sys.parse_iw(fixture("iw-not-connected.txt")), nil, "not connected")
eq(sys.parse_iw(nil), nil, "nothing")
eq(sys.parse_iw("Connected to x\n"), nil, "no ssid")
done()
)LUA"));
}

TEST_F(NetworkTest, the_parsers_of_nmcli_read_the_terse_output)
{
    EXPECT_TRUE(run(R"LUA(
local nm = require("nm")
local status = nm.parse_status(fixture("nmcli-device.txt"))
eq(status.wlan0.state, "connected", "wlan0")
eq(status.wlan0.connection, "Home Net", "connection with a space")
eq(status.wlan1.state, "unavailable", "unavailable")
eq(status.wlan1.connection, nil, "no connection")
eq(status.eth9.state, "unmanaged", "unmanaged")
eq(status.eth8.state, "connecting", "connecting (getting IP configuration)")
eq(status["68:4A:E9:33:94:F7"].type, "bt", "escaped colons of a name")
local wifi = nm.parse_wifi_list(fixture("nmcli-wifi.txt"))
eq(#wifi.wlan0, 7, "networks of wlan0")
eq(#wifi.wlan1, 1, "networks of wlan1")
eq(wifi.wlan0[1].ssid, "Home Net", "first")
yes(wifi.wlan0[1].active, "the active one")
eq(wifi.wlan0[1].signal, 86, "signal")
eq(wifi.wlan0[1].band, "5 GHz", "band")
eq(wifi.wlan0[1].rate, 433, "rate")
eq(wifi.wlan0[3].ssid, "Cafe: free", "an escaped colon in a name")
eq(wifi.wlan0[3].security, "--", "open")
yes(wifi.wlan0[4].enterprise, "802.1X")
yes(not wifi.wlan0[5].enterprise, "WPA1 WPA2")
eq(wifi.wlan0[7].ssid, "", "hidden")
local known = nm.parse_known(fixture("nmcli-known.txt"))
yes(known["Home Net"], "known")
yes(not known["Wired connection 1"], "a cable is not a Wi-Fi")
local all = nm.parse_permissions(fixture("nmcli-permissions.txt"))
yes(all.control and all.wifi and all.modify, "may do all")
local none = nm.parse_permissions(fixture("nmcli-permissions-none.txt"))
yes(not none.control and not none.wifi and not none.modify, "may do nothing")
eq(nm.explain({ok = true}), nil, "worked")
eq(nm.explain({status = 4}), "Could not connect (is the password right?)", "4")
eq(nm.explain({status = 3}), "It took too long", "3")
eq(nm.explain({timed_out = true, status = -1}), "It took too long", "timed out")
eq(nm.explain({status = 8}), "NetworkManager is not running", "8")
eq(nm.explain({status = 10}), "The network is not there any more", "10")
eq(nm.explain({status = -1, error = "No such file or directory"}), "nmcli: No such file or directory", "missing")
eq(nm.explain({status = 1}), "The change did not work", "other")
done()
)LUA"));
}

TEST_F(NetworkTest, networkmanager_adds_its_names_and_what_the_user_may_do)
{
    const fs::path root = tree.path / "fake";
    make_sys(root);
    make_programs(root / "bin");
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    EXPECT_TRUE(run("local ROOT = '" + root.string() + "'\n" + R"LUA(
local sys = require("sys")
sys.root = ROOT .. "/net/"
sys.wireless_file = ROOT .. "/wireless"
local nm = require("nm")
nm.read(function(adapters)
    local by = {}
    for _, a in ipairs(adapters) do by[a.name] = a end
    local wired, wifi = by.enp3s0, by.wlan0
    eq(wired.connection, "Wired connection 1", "the connection")
    eq(wired.state, "connected", "state")
    yes(wired.can_toggle and wired.can_connect, "the user may change it")
    eq(wired.reason, nil, "no reason")
    eq(wifi.wifi.ssid, "Home Net", "the active network")
    eq(wifi.wifi.signal, 86, "the signal of networkmanager")
    eq(wifi.wifi.band, "5 GHz", "band")
    eq(wifi.wifi.rate, 433, "rate")
    eq(wifi.wifi.security, "WPA2", "security")
    eq(wifi.radio, true, "radio")
    yes(wifi.can_scan and wifi.can_connect, "wifi may connect")
    eq(#wifi.networks, 7, "the networks in range")
    local saved = {}
    for _, net in ipairs(wifi.networks) do
        if net.known then saved[#saved + 1] = net.ssid end
    end
    eq(list(table.unpack(saved)), "Home Net,Home Net,Neighbour,Neighbour", "the saved networks")
    eq(by.wlan1.state, "unavailable", "wlan1")
    eq(#by.wlan1.networks, 1, "wlan1 networks")
    eq(by.eth9.state, "connected", "an unmanaged one keeps what the kernel says: the cable is in...")
    yes(not by.eth9.can_toggle and not by.eth9.can_connect, "...and may not be changed")
    eq(by.eth9.reason, "Not managed by NetworkManager", "unmanaged")
    eq(by.eth8.state, "connecting", "connecting")
    eq(by.down0.reason, "Not managed by NetworkManager", "not known to networkmanager")
    yes(not by.down0.can_toggle, "down0")
    nm.forget_permissions()
    done()
end)
)LUA"));
}

TEST_F(NetworkTest,
       a_user_without_permissions_may_not_change_the_network_and_is_told_so)
{
    const fs::path root = tree.path / "fake";
    make_sys(root);
    make_programs(root / "bin");
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    const EnvVar permissions("FAKE_PERMISSIONS", "nmcli-permissions-none.txt");
    const EnvVar radio("FAKE_RADIO", "disabled");
    EXPECT_TRUE(run("local ROOT = '" + root.string() + "'\n" + R"LUA(
local sys = require("sys")
sys.root = ROOT .. "/net/"
sys.wireless_file = ROOT .. "/wireless"
local nm = require("nm")
nm.read(function(adapters)
    local by = {}
    for _, a in ipairs(adapters) do by[a.name] = a end
    yes(not by.enp3s0.can_toggle and not by.enp3s0.can_connect, "wired may not")
    eq(by.enp3s0.reason, "You may not change the network", "wired reason")
    yes(not by.wlan0.can_toggle and not by.wlan0.can_connect, "wifi may not")
    eq(by.wlan0.radio, false, "the radio is off")
    nm.forget_permissions()
    done()
end)
)LUA"));
}

TEST_F(NetworkTest, networkmanager_that_does_not_run_leaves_a_reason)
{
    const fs::path root = tree.path / "fake";
    make_sys(root);
    make_programs(root / "bin");
    // an nmcli that fails like one without the daemon: exit status 8
    std::ofstream(root / "bin" / "nmcli") << "#!/bin/sh\nexit 8\n";
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    EXPECT_TRUE(run("local ROOT = '" + root.string() + "'\n" + R"LUA(
local sys = require("sys")
sys.root = ROOT .. "/net/"
sys.wireless_file = ROOT .. "/wireless"
local nm = require("nm")
nm.read(function(adapters)
    eq(#adapters, 6, "the adapters are still there")
    eq(adapters[1].reason, "NetworkManager is not running", "reason")
    yes(not adapters[1].can_toggle, "no changes")
    done()
end)
)LUA"));
}

TEST_F(NetworkTest, there_is_nothing_to_show_without_adapters)
{
    const fs::path root = tree.path / "fake";
    fs::create_directories(root / "net");
    make_programs(root / "bin");
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    EXPECT_TRUE(run("local ROOT = '" + root.string() + "'\n" + R"LUA(
local sys = require("sys")
sys.root = ROOT .. "/net/"
sys.wireless_file = ROOT .. "/none"
local nm = require("nm")
nm.read(function(adapters)
    eq(#adapters, 0, "nm")
    sys.read(function(again)
        eq(#again, 0, "sys")
        done()
    end)
end)
)LUA"));
}

TEST_F(NetworkTest,
       changes_are_made_by_nmcli_and_a_password_is_not_in_its_arguments)
{
    const fs::path root = tree.path / "fake";
    make_programs(root / "bin");
    const fs::path calls = root / "calls.txt";
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    const EnvVar log("FAKE_CALLS", calls.string());
    const EnvVar stdin_too("FAKE_STDIN", "1");
    EXPECT_TRUE(run(R"LUA(
local nm = require("nm")
local wlan = {name = "wlan0", kind = "wifi"}
local eth = {name = "enp3s0", kind = "ethernet"}
nm.toggle(eth, false, function(problem)
    eq(problem, nil, "turn off")
    nm.toggle(eth, true, function()
        nm.toggle(wlan, false, function()
            nm.toggle(wlan, true, function()
                nm.disconnect(wlan, function(problem2)
                    eq(problem2, nil, "disconnect")
                    nm.connect(wlan, {ssid = "Neighbour", known = true, security = "WPA2"}, nil, function(p3)
                        eq(p3, nil, "saved network")
                        nm.connect(wlan, {ssid = "NewNet", security = "WPA2"}, "s3cret pass", function(p4)
                            eq(p4, nil, "new network")
                            nm.connect(wlan, {ssid = "Cafe", security = "--"}, nil, function(p5)
                                eq(p5, nil, "open network")
                                nm.connect(wlan, {ssid = "NewNet", security = "WPA2"}, "", function(p6)
                                    eq(p6, "A password is needed", "no password")
                                    nm.connect(wlan, {ssid = "--help", security = "--"}, nil, function(p7)
                                        eq(p7, "A name that starts with a dash is not supported", "a dash")
                                        nm.scan(wlan, done)
                                    end)
                                end)
                            end)
                        end)
                    end)
                end)
            end)
        end)
    end)
end)
)LUA"));
    std::ifstream in(calls);
    const std::string written((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    const std::string new_net =
            "--ask -w 30 device wifi connect NewNet ifname wlan0\n";
    for (const std::string& wanted : std::vector<std::string>{
                 "device disconnect enp3s0\n", "device connect enp3s0\n",
                 "radio wifi off\n", "radio wifi on\n",
                 "device disconnect wlan0\n",
                 "-w 30 connection up id Neighbour ifname wlan0\n",
                 new_net + "stdin=s3cret pass\n",
                 "--ask -w 30 device wifi connect Cafe ifname wlan0\nstdin=\n",
                 "device wifi rescan ifname wlan0\n"}) {
        EXPECT_NE(written.find(wanted), std::string::npos) << wanted << "\nin\n"
                                                           << written;
    }
    // nothing for a name that starts with a dash
    EXPECT_EQ(written.find("--help"), std::string::npos) << written;
    // the password is on the standard input only, nowhere else
    size_t seen = 0;
    for (size_t at = written.find("s3cret"); at != std::string::npos;
         at = written.find("s3cret", at + 1)) {
        ++seen;
    }
    EXPECT_EQ(seen, 1U) << written;
    EXPECT_NE(written.find("stdin=s3cret pass"), std::string::npos);
}

TEST_F(NetworkTest, a_new_network_that_did_not_connect_is_not_saved)
{
    const fs::path root = tree.path / "fake";
    make_programs(root / "bin");
    const fs::path calls = root / "calls.txt";
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    const EnvVar log("FAKE_CALLS", calls.string());
    const EnvVar status("FAKE_STATUS", "4");
    EXPECT_TRUE(run(R"LUA(
local nm = require("nm")
nm.connect({name = "wlan0", kind = "wifi"}, {ssid = "NewNet", security = "WPA2"}, "wrong", function(problem)
    eq(problem, "Could not connect (is the password right?)", "the reason is kept")
    done()
end)
)LUA"));
    std::ifstream in(calls);
    const std::string written((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
    EXPECT_NE(written.find("connection delete id NewNet\n"), std::string::npos)
            << written;
}

TEST_F(NetworkTest, a_change_that_failed_says_why)
{
    const fs::path root = tree.path / "fake";
    make_programs(root / "bin");
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    const EnvVar status("FAKE_STATUS", "4");
    EXPECT_TRUE(run(R"LUA(
local nm = require("nm")
local wlan = {name = "wlan0", kind = "wifi"}
nm.connect(wlan, {ssid = "X", known = true}, nil, function(problem)
    eq(problem, "Could not connect (is the password right?)", "status 4")
    nm.toggle({name = "enp3s0", kind = "ethernet"}, false, function(problem2)
        eq(problem2, "Could not connect (is the password right?)", "same status")
        done()
    end)
end)
)LUA"));
}

TEST_F(NetworkTest, ifconfig_of_the_bsd_systems_is_read)
{
    EXPECT_TRUE(run(R"LUA(
local bsd = require("bsd")
local function by_name(adapters)
    local by = {}
    for _, a in ipairs(adapters) do by[a.name] = a end
    return by
end

-- FreeBSD, the output of a real machine: the loopback is not an adapter
local free = bsd.parse(fixture("freebsd-ethernet.txt"))
eq(#free, 1, "freebsd adapters")
eq(free[1].name, "em0", "freebsd name")
eq(free[1].kind, "ethernet", "freebsd kind")
eq(free[1].state, "connected", "freebsd state")
eq(free[1].speed, 1000, "freebsd speed")
eq(free[1].ipv4[1], "192.168.122.70/24", "freebsd ipv4 from the hex netmask")
eq(free[1].ipv6[1], "fe80::5054:ff:fe24:6874/64", "freebsd ipv6 without the zone")
yes(not free[1].can_toggle, "freebsd cannot change")

-- OpenBSD: enc0 and pflog0 are not cards
local open = bsd.parse(fixture("openbsd-ethernet.txt"))
eq(#open, 1, "openbsd adapters")
eq(open[1].name, "em0", "openbsd name")
eq(open[1].speed, 1000, "openbsd speed")
eq(open[1].ipv4[1], "192.168.122.179/24", "openbsd ipv4")
eq(open[1].state, "connected", "openbsd state")

-- NetBSD: the address has the length after a slash
local net = bsd.parse(fixture("netbsd-ethernet.txt"))
eq(#net, 1, "netbsd adapters")
eq(net[1].name, "wm0", "netbsd name")
eq(net[1].ipv4[1], "192.168.122.55/24", "netbsd ipv4")
eq(net[1].ipv6[1], "fe80::a3e:7384:4360:eba1/64", "netbsd ipv6")
eq(net[1].speed, 1000, "netbsd speed")

-- FreeBSD Wi-Fi (written by hand): a cable that is out, a card that is
-- associated and one that is not
local f = by_name(bsd.parse(fixture("freebsd-wifi.txt")))
eq(f.em0.state, "disconnected", "no carrier")
eq(f.wlan0.kind, "wifi", "wlan0")
eq(f.wlan0.state, "connected", "associated")
eq(f.wlan0.wifi.ssid, "My Home", "a quoted name")
eq(f.wlan0.wifi.band, "5 GHz", "band from the frequency")
eq(f.wlan0.wifi.rate, 54, "rate from the media")
eq(f.wlan0.wifi.signal, nil, "freebsd does not tell the signal")
eq(f.wlan0.ipv4[1], "192.168.1.20/24", "wlan0 ipv4")
eq(f.wlan1.state, "disconnected", "not associated")
eq(f.wlan1.wifi, nil, "no network")

-- OpenBSD Wi-Fi (written by hand): the percent of the signal is there
local o = by_name(bsd.parse(fixture("openbsd-wifi.txt")))
eq(o.iwx0.wifi.ssid, "My Home", "openbsd name")
eq(o.iwx0.wifi.signal, 79, "openbsd signal")
eq(o.iwx0.wifi.band, "2.4 GHz", "band from the channel")
eq(o.iwn0.state, "disabled", "not up")

-- an empty text and a text of something else
eq(#bsd.parse(""), 0, "empty")
eq(#bsd.parse("hello\nworld\n"), 0, "garbage")

-- the default routes of the three systems: the column is the one of the header
yes(bsd.parse_default_routes(fixture("netstat-freebsd.txt")).em0 ~= nil, "freebsd route")
yes(bsd.parse_default_routes(fixture("netstat-openbsd.txt")).em0 ~= nil, "openbsd route")
yes(bsd.parse_default_routes(fixture("netstat-netbsd.txt")).wm0 ~= nil, "netbsd route")
eq(bsd.parse_default_routes(fixture("netstat-netbsd.txt")).lo0, nil, "not the loopback")
done()
)LUA"));
}

TEST_F(NetworkTest, the_panel_shows_the_cable_the_signal_or_nothing)
{
    EXPECT_TRUE(run(R"LUA(
local view = require("view")
local function wifi(signal, ssid)
    return {name = "wlan0", kind = "wifi", state = "connected", metric = 600,
            wifi = {ssid = ssid or "Home", signal = signal}}
end
local wired = {name = "enp3s0", kind = "ethernet", state = "connected", metric = 100}

eq(view.panel({}).visible, false, "no adapters, nothing")
local p = view.panel({wired})
eq(p.icon, "network-wired", "cable")
eq(p.under, nil, "no icon under a cable")
p = view.panel({wired, wifi(90)})
eq(p.icon, "network-wired", "the cable has the better route")
p = view.panel({wifi(90)})
eq(p.icon, "network-wireless-signal-excellent", "full signal")
eq(p.under, "network-wireless-signal-excellent", "the full symbol dimmed under it")
p = view.panel({wifi(60)})
eq(p.icon, "network-wireless-signal-good", "good")
eq(p.under, "network-wireless-signal-excellent", "under good")
eq(view.panel({wifi(40)}).icon, "network-wireless-signal-ok", "ok")
eq(view.panel({wifi(10)}).icon, "network-wireless-signal-weak", "weak")
p = view.panel({wifi(2)})
eq(p.icon, nil, "no level")
eq(p.under, "network-wireless-signal-excellent", "only the dimmed one when there is no signal")
p = view.panel({{name = "wlan0", kind = "wifi", state = "connected", wifi = {ssid = "x"}}})
eq(p.icon, "network-wireless-signal-excellent", "a signal that is not known is shown as connected")
eq(p.under, nil, "without the dimmed one")
eq(view.panel({wifi(90, "Home")}, {ssid = true}).text, "Home", "the name when asked")
eq(view.panel({wifi(90, "Home")}, {ssid = false}).text, "", "no name")
eq(view.panel({wifi(90, "A very very long name of a network")}, {ssid = true}).text, "A very very long…", "cut")
eq(view.panel({wired}, {ssid = true}).text, "", "no name for a cable")

local off = {name = "wlan0", kind = "wifi", state = "disconnected"}
eq(view.panel({off}).icon, "network-wireless-offline", "wifi, not connected")
eq(view.panel({off, {name = "enp3s0", kind = "ethernet", state = "disconnected"}}).icon,
   "network-wireless-offline", "wifi is the one to show when none is connected")
eq(view.panel({{name = "enp3s0", kind = "ethernet", state = "disconnected"}}).icon,
   "network-wired-disconnected", "cable out")
p = view.panel({{name = "wlan0", kind = "wifi", state = "connecting"}})
eq(p.icon, nil, "connecting: no level")
eq(p.under, "network-wireless-signal-excellent", "connecting: the dimmed symbol")
done()
)LUA"));
}

TEST_F(NetworkTest,
       the_window_lists_the_adapters_with_their_addresses_and_actions)
{
    EXPECT_TRUE(run(R"LUA(
local view = require("view")
local calls = {}
local ui = {on = {
    toggle = function(a) calls[#calls + 1] = "toggle:" .. a.name end,
    connect = function(a, n) calls[#calls + 1] = "connect:" .. a.name .. ":" .. n.ssid end,
    disconnect = function(a) calls[#calls + 1] = "disconnect:" .. a.name end,
    submit = function(t) calls[#calls + 1] = "submit:" .. t end,
    cancel = function() calls[#calls + 1] = "cancel" end,
}}
local function find(window, heading)
    for _, section in ipairs(window.sections) do
        if section.heading == heading then return section end
    end
end
local function row(section, label)
    for _, r in ipairs(section.rows) do
        if r.label == label then return r end
    end
end

local wired = {
    name = "enp3s0", kind = "ethernet", state = "connected", carrier = true, speed = 1000,
    connection = "Wired connection 1", ipv4 = {"192.168.1.23/24"},
    ipv6 = {"fe80::1/64", "2a01:db8::7c1/64", "2a01:db8::7c2/64", "2a01:db8::7c3/64", "2a01:db8::7c4/64"},
    can_toggle = true, can_connect = true, metric = 100,
}
local wifi = {
    name = "wlan0", kind = "wifi", state = "connected", carrier = true, radio = true,
    ipv4 = {"10.0.0.15/24"}, ipv6 = {}, can_toggle = true, can_connect = true, can_scan = true,
    wifi = {ssid = "Home", signal = 70, band = "5 GHz", rate = 433, security = "WPA2"},
    networks = {
        {ssid = "Home", signal = 70, security = "WPA2"},
        {ssid = "Cafe", signal = 60, security = "--"},
        {ssid = "Work", signal = 50, security = "WPA2 802.1X", enterprise = true},
        {ssid = "Saved", signal = 40, security = "WPA2", known = true, band = "2.4 GHz"},
        {ssid = "Far", signal = 3, security = "WPA2"},
    },
}
local window = view.popup({wired, wifi}, ui, "lock")
eq(window.title, "Network", "title")
eq(window.subtitle, "Connected by cable", "the cable is the main one")
eq(window.icon, "network-wired", "header icon")

local e = find(window, "Ethernet · enp3s0")
yes(e, "an ethernet section")
eq(e.rows[1].label, "Connected", "state")
eq(e.rows[1].value, "1 Gb/s", "speed")
eq(e.rows[1].icon, "network-wired", "icon")
eq(row(e, "Connection").value, "Wired connection 1", "connection")
local ipv4, ipv6 = row(e, "IPv4"), row(e, "IPv6")
eq(ipv4.value, "192.168.1.23/24", "ipv4")
eq(ipv6.value, "2a01:db8::7c1/64", "the first global ipv6 (link-local hidden)")
local count = 0
for _, r in ipairs(e.rows) do if r.label == "" then count = count + 1 end end
eq(count, 2, "three ipv6 addresses at most: two without a label")
row(e, "Turn off").on_click()
eq(calls[#calls], "toggle:enp3s0", "turning off")

local w = find(window, "Wi-Fi · wlan0")
yes(w, "a wifi section")
eq(w.rows[1].label, "Home", "the connected network")
eq(w.rows[1].value, "Connected", "connected")
eq(w.rows[1].icon, "network-wireless-signal-good", "its signal")
eq(row(w, "Signal").value, "70 %", "signal")
eq(row(w, "Band").value, "5 GHz", "band")
eq(row(w, "Speed").value, "433 Mb/s", "speed")
eq(row(w, "Security").value, "WPA2", "security")
eq(row(w, "Security").badge, "lock", "the lock")
eq(row(w, "IPv4").value, "10.0.0.15/24", "ipv4")
row(w, "Disconnect").on_click()
eq(calls[#calls], "disconnect:wlan0", "disconnect")
row(w, "Turn Wi-Fi off").on_click()
eq(calls[#calls], "toggle:wlan0", "radio")

local n = find(window, "Available networks")
yes(n, "the networks in range")
eq(#n.rows, 4, "the connected one is not in the list")
eq(n.rows[1].label, "Cafe", "the strongest first")
eq(n.rows[1].badge, nil, "open: no lock")
eq(n.rows[1].value, "", "no band known")
eq(n.rows[2].label, "Work", "second")
eq(n.rows[2].value, "Needs a login", "enterprise")
yes(n.rows[2].dim and not n.rows[2].on_click, "enterprise cannot be used")
eq(n.rows[2].badge, "lock", "lock")
eq(n.rows[3].label, "Saved", "saved")
eq(n.rows[3].value, "2.4 GHz · Saved", "saved with the band")
eq(n.rows[4].label, "Far", "far")
eq(n.rows[4].icon, "network-wireless-signal-weak", "no signal is weak")
n.rows[1].on_click()
eq(calls[#calls], "connect:wlan0:Cafe", "connect")
yes(n.rows[1].id and n.rows[1].id ~= n.rows[3].id, "rows have an id")

-- a user who may not change anything
wifi.can_connect, wifi.can_toggle, wifi.reason = false, false, "You may not change the network"
window = view.popup({wifi}, ui, "lock")
w = find(window, "Wi-Fi · wlan0")
eq(row(w, "Disconnect"), nil, "no disconnect")
eq(row(w, "Turn Wi-Fi off"), nil, "no switch")
yes(row(w, "You may not change the network").dim, "the reason is shown")
n = find(window, "Available networks")
yes(n.rows[1].dim and not n.rows[1].on_click, "the networks are dim")
done()
)LUA"));
}

TEST_F(NetworkTest, the_window_has_the_states_of_a_cable_a_radio_and_a_password)
{
    EXPECT_TRUE(run(R"LUA(
local view = require("view")
local ui = {on = {toggle = function() end, connect = function() end, disconnect = function() end,
    submit = function() end, cancel = function() end}}
local function find(window, heading)
    for _, section in ipairs(window.sections) do
        if section.heading == heading then return section end
    end
end
local function row(section, label)
    for _, r in ipairs(section.rows) do
        if r.label == label then return r end
    end
end

-- a cable that is out has no switch, a cable that is turned off has
local out = {name = "e0", kind = "ethernet", state = "unavailable", carrier = false, can_toggle = true}
local e = find(view.popup({out}, ui), "Ethernet · e0")
eq(e.rows[1].label, "Cable unplugged", "unplugged")
eq(e.rows[1].icon, "network-wired-disconnected", "icon")
eq(row(e, "Turn on"), nil, "nothing to turn on")
local off = {name = "e0", kind = "ethernet", state = "disconnected", carrier = true, can_toggle = true}
e = find(view.popup({off}, ui), "Ethernet · e0")
eq(e.rows[1].label, "Disconnected", "disconnected")
yes(row(e, "Turn on"), "a switch")
local connecting = {name = "e0", kind = "ethernet", state = "connecting", can_toggle = true}
e = find(view.popup({connecting}, ui), "Ethernet · e0")
eq(e.rows[1].label, "Connecting…", "connecting")
yes(row(e, "Turn off"), "it can be stopped")
local locked = {name = "e0", kind = "ethernet", state = "connected", can_toggle = false, reason = "Managed by the system"}
e = find(view.popup({locked}, ui), "Ethernet · e0")
yes(row(e, "Managed by the system").dim, "a reason, not a switch")
eq(row(e, "Turn off"), nil, "no switch")

-- a Wi-Fi that is off has the switch and no list
local radio = {name = "w0", kind = "wifi", state = "unavailable", radio = false, can_toggle = true, can_scan = true}
local window = view.popup({radio}, ui)
local w = find(window, "Wi-Fi · w0")
eq(w.rows[1].label, "Wi-Fi is off", "off")
yes(row(w, "Turn Wi-Fi on"), "turn on")
eq(find(window, "Available networks"), nil, "no list while it is off")
eq(window.subtitle, "Not connected", "summary")

-- not connected: the list says that it searches, or that it found nothing
local idle = {name = "w0", kind = "wifi", state = "disconnected", radio = true, can_scan = true, networks = {}}
window = view.popup({idle}, {on = ui.on, scanning = true})
eq(find(window, "Wi-Fi · w0").rows[1].label, "Not connected", "not connected")
eq(find(window, "Available networks").rows[1].label, "Searching…", "searching")
window = view.popup({idle}, {on = ui.on})
eq(find(window, "Available networks").rows[1].label, "No networks found", "nothing found")
eq(find(view.popup({{name = "w0", kind = "wifi", state = "connecting", radio = true}}, ui), "Wi-Fi · w0").rows[1].label,
   "Connecting…", "connecting")
-- a system that cannot look for networks has no list
eq(find(view.popup({{name = "w0", kind = "wifi", state = "disconnected"}}, ui), "Available networks"), nil,
   "no list without a scan")

-- a message and the question for a password come first
local asking = {ssid = "Home"}
window = view.popup({idle}, {on = {submit = function() end, cancel = function() end, toggle = function() end,
    connect = function() end, disconnect = function() end}, message = "Wrong password", asking = asking})
eq(window.sections[1].rows[1].label, "Wrong password", "message")
local q = window.sections[2]
eq(q.heading, "Connect to Home", "question")
yes(q.rows[1].entry and q.rows[1].secret, "a secret field")
yes(q.rows[1].on_submit, "its action")
yes(q.rows[2].on_click, "cancel")

-- more adapters than the window can hold: the first ones are shown
local many = {}
for i = 1, 6 do
    many[i] = {name = "w" .. i, kind = "wifi", state = "disconnected", radio = true, can_scan = true, networks = {}}
end
window = view.popup(many, {on = ui.on, message = "x"})
yes(#window.sections <= 8, "at most eight sections")
done()
)LUA"));
}

TEST_F(NetworkTest, the_plugin_starts_and_looks_without_an_error)
{
    // plugin.lua itself, with the programs of the system replaced
    const fs::path root = tree.path / "fake";
    make_programs(root / "bin");
    const EnvVar path("PATH", (root / "bin").string() + ":" + getenv("PATH"));
    tree.add("net", "tiny.log('replaced below')");
    for (const auto& entry : fs::directory_iterator(source)) {
        fs::copy_file(entry.path(), tree.path / "net" / entry.path().filename(),
                      fs::copy_options::overwrite_existing);
    }
    // the plugin reads /sys: only what is there decides what happens, so
    // the script only checks that it runs without an error in the log
    PluginHost host(services);
    host.apply(tree.find(), config);
    loop.run_until([&] { return false; }, 300ms);
    EXPECT_EQ(host.error_of("net"), "");
    for (const std::string& line : lines) {
        EXPECT_EQ(line.find("attempt to"), std::string::npos) << line;
        EXPECT_EQ(line.find("stack traceback"), std::string::npos) << line;
    }
    host.stop();
}

#endif // TINY_HAVE_LUA
