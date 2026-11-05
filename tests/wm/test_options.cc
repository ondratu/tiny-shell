#include <gtest/gtest.h>

#include "wm/config.h"
#include "wm/options.h"

using tiny::wm::Options;
using tiny::wm::parse_options;

TEST(OptionsTest, no_arguments_run_the_window_manager)
{
    const Options o = parse_options({});

    EXPECT_EQ(o.action, Options::Action::Run);
    EXPECT_FALSE(o.replace);
}

TEST(OptionsTest, replace)
{
    const Options o = parse_options({"--replace"});

    EXPECT_EQ(o.action, Options::Action::Run);
    EXPECT_TRUE(o.replace);
}

TEST(OptionsTest, help_version_and_info)
{
    EXPECT_EQ(parse_options({"--help"}).action, Options::Action::Help);
    EXPECT_EQ(parse_options({"-h"}).action, Options::Action::Help);
    EXPECT_EQ(parse_options({"--version"}).action, Options::Action::Version);
    EXPECT_EQ(parse_options({"-V"}).action, Options::Action::Version);
    EXPECT_EQ(parse_options({"--info"}).action, Options::Action::Info);
}

TEST(OptionsTest, help_wins_over_the_other_actions)
{
    EXPECT_EQ(parse_options({"--version", "--help", "--info"}).action,
              Options::Action::Help);
    EXPECT_EQ(parse_options({"--info", "--version"}).action,
              Options::Action::Version);
    EXPECT_EQ(parse_options({"--version", "--info"}).action,
              Options::Action::Version);
}

TEST(OptionsTest, replace_works_with_the_information_options)
{
    const Options o = parse_options({"--replace", "--info"});

    EXPECT_EQ(o.action, Options::Action::Info);
    EXPECT_TRUE(o.replace);
}

TEST(OptionsTest, unknown_option_is_an_error)
{
    const Options o = parse_options({"--replace", "--bogus"});

    EXPECT_EQ(o.action, Options::Action::Error);
    EXPECT_EQ(o.error, "unknown option '--bogus'");
}

TEST(OptionsTest, an_argument_is_an_error)
{
    const Options o = parse_options({"file"});

    EXPECT_EQ(o.action, Options::Action::Error);
    EXPECT_EQ(o.error, "unexpected argument 'file'");
}

TEST(OptionsTest, an_error_wins_over_help)
{
    EXPECT_EQ(parse_options({"--help", "--bogus"}).action,
              Options::Action::Error);
}

TEST(OptionsTest, version_names_the_program)
{
    EXPECT_EQ(tiny::wm::version_text().rfind("tiny-shell ", 0), 0u);
}

TEST(OptionsTest, usage_lists_every_option)
{
    const std::string text = tiny::wm::usage_text("tiny");

    EXPECT_EQ(text.rfind("Usage: tiny ", 0), 0u);
    for (const char* option :
         {"--replace", "--info", "--version", "--help", "-V", "-h"}) {
        EXPECT_NE(text.find(option), std::string::npos) << option;
    }
}

TEST(OptionsTest, info_has_the_settings_and_the_directories)
{
    tiny::wm::Config config;
    config.scale = 1.5;
    config.reboot_command = "my-reboot";
    config.poweroff_command = "my-poweroff";
    config.font = "My Font";
    config.terminal_line = "my-term -e";
    config.style = tiny::Variant::Light;
    config.wallpaper_file = "/p/sky.jpg";
    const std::string text = tiny::wm::info_text(
            config, {"/etc/xdg/tiny-shell.ini"}, {"/a/applications"});

    EXPECT_NE(text.find("1.5"), std::string::npos);
    EXPECT_NE(text.find("Colors of the theme: light"), std::string::npos);
    EXPECT_NE(text.find("my-reboot"), std::string::npos);
    EXPECT_NE(text.find("My Font"), std::string::npos);
    EXPECT_NE(text.find("my-term -e"), std::string::npos);
    EXPECT_NE(text.find("my-poweroff"), std::string::npos);
    EXPECT_NE(text.find("Wallpaper: /p/sky.jpg"), std::string::npos);
    EXPECT_NE(tiny::wm::info_text(tiny::wm::Config(), {}, {})
                      .find("Wallpaper: none"),
              std::string::npos);
    EXPECT_NE(text.find("Clock: %a %x %R"), std::string::npos);
    config.clock_format.clear();
    EXPECT_NE(tiny::wm::info_text(config, {}, {}).find("Clock: none"),
              std::string::npos);
    EXPECT_NE(text.find("/etc/xdg/tiny-shell.ini"), std::string::npos);
    EXPECT_NE(text.find("/a/applications"), std::string::npos);
    EXPECT_NE(text.find(tiny::wm::version_text()), std::string::npos);
}

TEST(OptionsTest, the_removed_command_options_are_unknown)
{
    for (const char* option : {"--restart-command=x", "--shutdown-command=x"}) {
        EXPECT_EQ(parse_options({option}).action, Options::Action::Error)
                << option;
    }
}
