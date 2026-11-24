#include "host_fixture.h"

#include <filesystem>

#ifdef TINY_HAVE_LUA

// The readers of the battery plugin that are not Linux: the module of NetBSD
// is a function from the text of envstat to the charge and the state, so the
// test replaces plugin.lua by a script that feeds it the files of
// tests/plugin/battery/ and waits for the script to say that it is done.

namespace {

namespace fs = std::filesystem;

const char* const prelude = R"LUA(
local function eq(got, want, what)
    if got ~= want then
        tiny.log("FAIL " .. what .. ": got " .. tostring(got) .. ", want " .. tostring(want))
    end
end
local function fixture(name)
    return assert(tiny.read_file(tiny.plugin.dir .. "/fixtures/" .. name))
end
)LUA";

class BatteryTest : public HostTest {
  protected:
    void SetUp() override
    {
        source = "data/tiny-shell/plugins/battery";
        if (!fs::exists(source)) {
            GTEST_SKIP() << "run from the directory of the sources";
        }
    }

    //! Runs the script in the plugin; true when it said DONE and nothing FAIL
    bool run(const std::string& body)
    {
        const fs::path dir = tree.path / "bat";
        tree.add("bat", std::string(prelude) + body + "\ntiny.log('DONE')\n");
        for (const auto& entry : fs::directory_iterator(source)) {
            if (entry.path().extension() == ".lua" &&
                entry.path().filename() != "plugin.lua") {
                fs::copy_file(entry.path(), dir / entry.path().filename(),
                              fs::copy_options::overwrite_existing);
            }
        }
        fs::create_directories(dir / "fixtures");
        for (const auto& entry :
             fs::directory_iterator("tests/plugin/battery")) {
            fs::copy_file(entry.path(),
                          dir / "fixtures" / entry.path().filename(),
                          fs::copy_options::overwrite_existing);
        }
        PluginHost host(services);
        host.apply(tree.find(), config);
        const bool finished =
                loop.run_until([&] { return logged("plugin bat: DONE"); });
        for (const std::string& line : lines) {
            if (line.find("FAIL") != std::string::npos) {
                ADD_FAILURE() << line;
            }
        }
        EXPECT_EQ(host.error_of("bat"), "");
        EXPECT_TRUE(finished) << "the script did not finish";
        host.stop();
        return finished;
    }

    fs::path source;
};

} // namespace

TEST_F(BatteryTest, netbsd_on_mains_and_not_charging_is_full)
{
    EXPECT_TRUE(run(R"LUA(
local info = require("netbsd").parse(fixture("netbsd-mains.txt"))
eq(info.percent, 96, "the percentage of the charge")
eq(info.state, "full", "on mains, not charging")
)LUA"));
}

TEST_F(BatteryTest, netbsd_charging_is_charging)
{
    EXPECT_TRUE(run(R"LUA(
local info = require("netbsd").parse(fixture("netbsd-charging.txt"))
eq(info.percent, 71, "the percentage of the charge")
eq(info.state, "charging", "charging")
)LUA"));
}

TEST_F(BatteryTest, netbsd_without_the_adapter_connected_is_discharging)
{
    EXPECT_TRUE(run(R"LUA(
local info = require("netbsd").parse(fixture("netbsd-discharging.txt"))
eq(info.percent, 50, "the percentage of the charge")
eq(info.state, "discharging", "on battery")
)LUA"));
}

TEST_F(BatteryTest, netbsd_without_a_battery_or_with_an_empty_bay_shows_nothing)
{
    EXPECT_TRUE(run(R"LUA(
local netbsd = require("netbsd")
eq(netbsd.parse(fixture("netbsd-no-battery.txt")), nil, "no battery device")
eq(netbsd.parse(fixture("netbsd-empty-bay.txt")), nil, "battery not present")
eq(netbsd.parse(""), nil, "no output")
)LUA"));
}

TEST_F(BatteryTest, netbsd_averages_two_batteries_and_one_charging_wins)
{
    EXPECT_TRUE(run(R"LUA(
local text = fixture("netbsd-discharging.txt") ..
    fixture("netbsd-charging.txt"):gsub("acpibat0", "acpibat1"):gsub("acpiacad0", "acpiacad1")
local info = require("netbsd").parse(text)
eq(info.percent, 61, "the average of 50 and 71")
eq(info.state, "charging", "one of them charges")
)LUA"));
}

#endif // TINY_HAVE_LUA
