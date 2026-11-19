#include <gtest/gtest.h>

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "plugin/plugin_info.h"

namespace fs = std::filesystem;
using namespace tiny::plugin;

namespace {

//! A directory that is removed with the object
class TempDir {
  public:
    TempDir()
    {
        static int counter = 0;
        path = fs::temp_directory_path() /
               ("tiny-plugins-" + std::to_string(getpid()) + "-" +
                std::to_string(counter++));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() { fs::remove_all(path); }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    //! A plugin in the directory: the files and what is in them
    std::string plugin(const std::string& id, const std::string& ini,
                       bool lua = true) const
    {
        const fs::path dir = path / id;
        fs::create_directories(dir);
        if (!ini.empty()) {
            std::ofstream(dir / "plugin.ini") << ini;
        }
        if (lua) {
            std::ofstream(dir / "plugin.lua") << "-- code\n";
        }
        return dir.string();
    }

    fs::path path;
};

const char* const weather =
        "[plugin]\nname = Weather\ndescription = The weather\nversion = 1.2\n"
        "author = A Name\nurl = https://example.org\napi = 1\nenabled = yes\n"
        "settings = location;units;days;fast\n"
        "[setting.location]\ntype = text\nlabel = Location\nnote = A town\n"
        "default = Prague\n"
        "[setting.units]\ntype = choice\nchoices = metric=Celsius; imperial "
        "= Fahrenheit;plain\ndefault = imperial\n"
        "[setting.days]\ntype = number\nmin = 1\nmax = 7\ndefault = 3\n"
        "[setting.fast]\ntype = bool\n";

} // namespace

TEST(PluginInfoTest, reads_the_metadata_and_the_settings)
{
    const PluginInfo info = parse_plugin_info("weather", "/nonexistent",
                                              Origin::System, weather);
    EXPECT_EQ(info.id, "weather");
    EXPECT_EQ(info.name, "Weather");
    EXPECT_EQ(info.description, "The weather");
    EXPECT_EQ(info.version, "1.2");
    EXPECT_EQ(info.author, "A Name");
    EXPECT_EQ(info.url, "https://example.org");
    EXPECT_TRUE(info.default_enabled);
    ASSERT_EQ(info.settings.size(), 4u);

    const SettingSpec& location = info.settings[0];
    EXPECT_EQ(location.key, "location");
    EXPECT_EQ(location.type, SettingType::Text);
    EXPECT_EQ(location.label, "Location");
    EXPECT_EQ(location.note, "A town");
    EXPECT_EQ(location.fallback, "Prague");

    const SettingSpec& units = info.settings[1];
    EXPECT_EQ(units.label, "units"); // the key when there is no label
    ASSERT_EQ(units.choices.size(), 3u);
    EXPECT_EQ(units.choices[0].value, "metric");
    EXPECT_EQ(units.choices[0].label, "Celsius");
    EXPECT_EQ(units.choices[1].value, "imperial");
    EXPECT_EQ(units.choices[1].label, "Fahrenheit");
    EXPECT_EQ(units.choices[2].label, "plain"); // no name: the value
    EXPECT_EQ(units.fallback, "imperial");

    const SettingSpec& days = info.settings[2];
    EXPECT_EQ(days.min, std::optional<double>(1));
    EXPECT_EQ(days.max, std::optional<double>(7));
    EXPECT_EQ(days.fallback, "3");
    EXPECT_EQ(info.settings[3].fallback, "no"); // a bool is off by default

    EXPECT_EQ(info.find_setting("days"), &info.settings[2]);
    EXPECT_EQ(info.find_setting("nothing"), nullptr);
}

TEST(PluginInfoTest, a_missing_lua_file_is_a_problem)
{
    TempDir dir;
    const std::string where = dir.plugin("a", "[plugin]\nname = A\n", false);
    EXPECT_EQ(parse_plugin_info("a", where, Origin::User, "[plugin]\n").problem,
              "plugin.lua is missing");
    EXPECT_TRUE(parse_plugin_info("a", dir.plugin("b", "", true), Origin::User,
                                  "[plugin]\n")
                        .problem.empty());
}

TEST(PluginInfoTest, the_defaults_of_a_plugin_that_says_little)
{
    TempDir dir;
    const PluginInfo info =
            parse_plugin_info("quiet", dir.plugin("quiet", "[plugin]\n"),
                              Origin::User, "[plugin]\n");
    EXPECT_EQ(info.name, "quiet");
    EXPECT_FALSE(info.default_enabled);
    EXPECT_EQ(info.api, 1);
    EXPECT_TRUE(info.settings.empty());
    EXPECT_TRUE(info.problem.empty());
}

TEST(PluginInfoTest, a_newer_interface_is_a_problem)
{
    const PluginInfo info = parse_plugin_info(
            "x", "/nonexistent", Origin::System, "[plugin]\napi = 2\n");
    EXPECT_NE(info.problem.find("interface 2"), std::string::npos);
    EXPECT_NE(parse_plugin_info("x", "/nonexistent", Origin::System,
                                "[plugin]\napi = one\n")
                      .problem.find("api must be"),
              std::string::npos);
}

TEST(PluginInfoTest, a_wrong_setting_is_a_problem_and_is_left_out)
{
    const auto problem = [](const std::string& text) {
        return parse_plugin_info("x", "/nonexistent", Origin::System, text)
                .problem;
    };
    EXPECT_NE(problem("[plugin]\nsettings = a\n[setting.a]\ntype = list\n")
                      .find("must be bool, choice, text or number"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a\n").find("must be bool"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = Bad\n").find("wrong name"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = enabled\n[setting.enabled]\n"
                      "type = bool\n")
                      .find("wrong name"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a;a\n[setting.a]\ntype = bool\n")
                      .find("twice"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a\n[setting.a]\ntype = choice\n")
                      .find("no choices"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a\n[setting.a]\ntype = number\n"
                      "min = 5\nmax = 1\n")
                      .find("range"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a\n[setting.a]\ntype = number\n"
                      "min = 1\ndefault = 0\n")
                      .find("default"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a\n[setting.a]\ntype = bool\n"
                      "default = maybe\n")
                      .find("default"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nsettings = a\n[setting.a]\ntype = choice\n"
                      "choices = x;y\ndefault = z\n")
                      .find("default"),
              std::string::npos);
    EXPECT_NE(problem("[plugin]\nname\n").find("line"), std::string::npos);
    const PluginInfo info =
            parse_plugin_info("x", "/nonexistent", Origin::System,
                              "[plugin]\nsettings = a;b\n[setting.a]\ntype = "
                              "list\n[setting.b]\ntype = bool\n");
    ASSERT_EQ(info.settings.size(), 1u);
    EXPECT_EQ(info.settings[0].key, "b");
}

TEST(PluginInfoTest, the_values_of_a_setting_are_checked_by_its_type)
{
    const PluginInfo info = parse_plugin_info("weather", "/nonexistent",
                                              Origin::System, weather);
    const SettingSpec& location = info.settings[0];
    const SettingSpec& units = info.settings[1];
    const SettingSpec& days = info.settings[2];
    const SettingSpec& fast = info.settings[3];
    EXPECT_TRUE(valid_setting_value(location, "Brno"));
    EXPECT_TRUE(valid_setting_value(location, ""));
    EXPECT_FALSE(valid_setting_value(location, "a\nb"));
    EXPECT_TRUE(valid_setting_value(units, "metric"));
    EXPECT_FALSE(valid_setting_value(units, "Celsius"));
    EXPECT_TRUE(valid_setting_value(days, "7"));
    EXPECT_TRUE(valid_setting_value(days, "2.5"));
    EXPECT_FALSE(valid_setting_value(days, "8"));
    EXPECT_FALSE(valid_setting_value(days, "0"));
    EXPECT_FALSE(valid_setting_value(days, "many"));
    EXPECT_FALSE(valid_setting_value(days, ""));
    EXPECT_TRUE(valid_setting_value(fast, "yes"));
    EXPECT_FALSE(valid_setting_value(fast, "true"));
}

TEST(PluginInfoTest, the_value_of_a_setting_is_the_one_of_the_user_when_valid)
{
    const PluginInfo info = parse_plugin_info("weather", "/nonexistent",
                                              Origin::System, weather);
    tiny::wm::Config config;
    EXPECT_EQ(setting_value(info.settings[0], config, "weather"), "Prague");
    config.plugins["weather"].values["location"] = "Brno";
    config.plugins["weather"].values["days"] = "99"; // out of the range
    EXPECT_EQ(setting_value(info.settings[0], config, "weather"), "Brno");
    EXPECT_EQ(setting_value(info.settings[2], config, "weather"), "3");
    EXPECT_EQ(setting_value(info.settings[0], config, "other"), "Prague");
}

TEST(PluginInfoTest, the_user_chooses_and_a_broken_plugin_never_runs)
{
    PluginInfo info;
    info.id = "p";
    tiny::wm::Config config;
    EXPECT_FALSE(plugin_enabled(info, config)); // off by default
    info.default_enabled = true;
    EXPECT_TRUE(plugin_enabled(info, config));
    config.plugins["p"].enabled = false;
    EXPECT_FALSE(plugin_enabled(info, config));
    config.plugins["p"].enabled = true;
    info.default_enabled = false;
    EXPECT_TRUE(plugin_enabled(info, config));
    info.problem = "broken";
    EXPECT_FALSE(plugin_enabled(info, config));
}

TEST(PluginInfoTest, versions_are_compared_by_their_numbers)
{
    EXPECT_EQ(compare_versions("1.0", "1.0"), 0);
    EXPECT_EQ(compare_versions("1.0", "1"), 0);
    EXPECT_EQ(compare_versions("1.9", "1.10"), -1);
    EXPECT_EQ(compare_versions("2", "1.99"), 1);
    EXPECT_EQ(compare_versions("", "0.1"), -1);
    EXPECT_EQ(compare_versions("1.2.3", "1.2.3"), 0);
}

TEST(PluginInfoTest, the_directories_follow_the_xdg_variables)
{
    const PluginDirs dirs =
            plugin_dirs("/home/u/.data", "/opt/share:/usr/share", "/home/u");
    EXPECT_EQ(dirs.user, "/home/u/.data/tiny-shell/plugins");
    ASSERT_EQ(dirs.system.size(), 2u);
    EXPECT_EQ(dirs.system[0], "/opt/share/tiny-shell/plugins");
    EXPECT_EQ(dirs.system[1], "/usr/share/tiny-shell/plugins");

    const PluginDirs defaults = plugin_dirs(nullptr, nullptr, "/home/u");
    EXPECT_EQ(defaults.user, "/home/u/.local/share/tiny-shell/plugins");
    ASSERT_EQ(defaults.system.size(), 2u);
    EXPECT_EQ(defaults.system[0], "/usr/local/share/tiny-shell/plugins");

    EXPECT_TRUE(plugin_dirs(nullptr, nullptr, nullptr).user.empty());
}

TEST(PluginInfoTest, finds_the_plugins_of_the_directories_by_their_ids)
{
    TempDir system;
    TempDir user;
    system.plugin("zeta", "[plugin]\nname = Zeta\n");
    system.plugin("alpha", "[plugin]\nname = Alpha\nversion = 1.0\n");
    user.plugin("mine", "[plugin]\nname = Mine\n");
    // not plugins: a file, a wrong id, a directory with nothing
    std::ofstream(system.path / "file.txt") << "x";
    system.plugin("Bad Id", "[plugin]\n");
    fs::create_directories(system.path / "empty");

    PluginDirs dirs;
    dirs.user = user.path.string();
    dirs.system = {system.path.string(), "/nonexistent"};
    const auto found = find_plugins(dirs);
    ASSERT_EQ(found.size(), 3u); // "empty" and "Bad Id" are not plugins
    EXPECT_EQ(found[0].id, "alpha");
    EXPECT_EQ(found[1].id, "mine");
    EXPECT_EQ(found[2].id, "zeta");
    EXPECT_EQ(found[0].origin, Origin::System);
    EXPECT_EQ(found[1].origin, Origin::User);
    EXPECT_EQ(found[0].name, "Alpha");
    EXPECT_EQ(found[0].dir, (system.path / "alpha").string());
    EXPECT_FALSE(found[0].shadows);
}

TEST(PluginInfoTest, a_plugin_of_the_user_replaces_the_system_one)
{
    TempDir system;
    TempDir user;
    system.plugin("clock", "[plugin]\nname = Clock\nversion = 1.0\n");
    user.plugin("clock", "[plugin]\nname = My clock\nversion = 1.5\n");
    PluginDirs dirs;
    dirs.user = user.path.string();
    dirs.system = {system.path.string()};
    const auto found = find_plugins(dirs);
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].origin, Origin::User);
    EXPECT_EQ(found[0].name, "My clock");
    EXPECT_TRUE(found[0].shadows);
    EXPECT_EQ(found[0].shadowed_version, "1.0");
}

TEST(PluginInfoTest, the_first_system_directory_wins_over_the_next_ones)
{
    TempDir first;
    TempDir second;
    first.plugin("clock", "[plugin]\nname = First\n");
    second.plugin("clock", "[plugin]\nname = Second\n");
    PluginDirs dirs;
    dirs.system = {first.path.string(), second.path.string()};
    const auto found = find_plugins(dirs);
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].name, "First");
    EXPECT_FALSE(found[0].shadows);
}

TEST(PluginInfoTest, a_plugin_without_its_metadata_is_listed_with_a_problem)
{
    TempDir user;
    user.plugin("lonely", "", true); // plugin.lua only
    PluginDirs dirs;
    dirs.user = user.path.string();
    const auto found = find_plugins(dirs);
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].problem, "plugin.ini is missing");
    EXPECT_EQ(found[0].name, "lonely");
}

TEST(PluginInfoTest, no_directory_means_no_plugin)
{
    EXPECT_TRUE(find_plugins({}).empty());
    PluginDirs dirs;
    dirs.user = "/nonexistent/tiny";
    dirs.system = {"/nonexistent/tiny2"};
    EXPECT_TRUE(find_plugins(dirs).empty());
}
