#include <gtest/gtest.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "wm/config.h"

using tiny::wm::Config;
using tiny::wm::parse_config;
using Strings = std::vector<std::string>;

TEST(ConfigTest, the_style_of_the_theme_is_dark_or_light)
{
    EXPECT_EQ(parse_config({}).style, tiny::Variant::Dark);
    EXPECT_EQ(parse_config({{"a", "[theme]\nstyle = Light\n"}}).style,
              tiny::Variant::Light);

    std::vector<std::string> problems;
    const Config c =
            parse_config({{"a", "[theme]\nstyle = blue\n"}}, &problems);

    EXPECT_EQ(c.style, tiny::Variant::Dark);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("[theme] style must be dark or light"),
              std::string::npos);
}

TEST(ConfigTest, the_defaults_work_without_any_file)
{
    const Config c = parse_config({});

    EXPECT_EQ(c.scale, 1.0);
    EXPECT_EQ(c.reboot_command, "systemctl reboot");
    EXPECT_EQ(c.poweroff_command, "systemctl poweroff");
    EXPECT_EQ(c.terminal, (Strings{"xterm", "-e"}));
    EXPECT_EQ(c.terminal_line, "xterm -e");
    EXPECT_EQ(c.font, "DejaVu Sans,sans");
    EXPECT_TRUE(c.favorites.empty());
}

TEST(ConfigTest, reads_the_font_and_keeps_the_text_of_the_terminal)
{
    const Config c =
            parse_config({{"a", "[theme]\nfont = DejaVu Sans Mono\n"
                                "[commands]\nterminal = \"my term\" -e\n"}});

    EXPECT_EQ(c.font, "DejaVu Sans Mono");
    EXPECT_EQ(c.terminal_line, "\"my term\" -e");
}

TEST(ConfigTest, an_empty_font_is_a_problem_and_keeps_the_default)
{
    Strings problems;

    const Config c = parse_config({{"a", "[theme]\nfont =\n"}}, &problems);

    EXPECT_EQ(c.font, "DejaVu Sans,sans");
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("font"), std::string::npos);
}

TEST(ConfigTest, a_wrong_terminal_keeps_the_text_of_the_default)
{
    Strings problems;

    const Config c =
            parse_config({{"a", "[commands]\nterminal = \"\n"}}, &problems);

    EXPECT_EQ(c.terminal_line, "xterm -e");
}

TEST(ConfigTest, the_scale_is_a_number_above_zero_up_to_sixteen)
{
    using tiny::wm::parse_scale;

    EXPECT_EQ(parse_scale("1.5"), 1.5);
    EXPECT_EQ(parse_scale("  2 "), 2.0);
    EXPECT_EQ(parse_scale("16"), 16.0);
    EXPECT_FALSE(parse_scale(""));
    EXPECT_FALSE(parse_scale("0"));
    EXPECT_FALSE(parse_scale("-1"));
    EXPECT_FALSE(parse_scale("16.5"));
    EXPECT_FALSE(parse_scale("2x"));
    EXPECT_FALSE(parse_scale("nan"));
}

TEST(ConfigTest, a_font_is_a_name_without_control_characters)
{
    using tiny::wm::valid_font;

    EXPECT_TRUE(valid_font("Noto Sans"));
    EXPECT_TRUE(valid_font(" Noto Sans "));
    EXPECT_FALSE(valid_font(""));
    EXPECT_FALSE(valid_font("   "));
    EXPECT_FALSE(valid_font("Noto\nSans"));
    EXPECT_FALSE(valid_font(std::string("Noto\0Sans", 9)));
}

TEST(ConfigTest, reads_the_settings)
{
    const Config c = parse_config(
            {{"a",
              "[theme]\nscale = 1.5\n"
              "[commands]\nreboot = sudo reboot\npoweroff = doas poweroff\n"
              "terminal = foot -e\n"}});

    EXPECT_EQ(c.scale, 1.5);
    EXPECT_EQ(c.reboot_command, "sudo reboot");
    EXPECT_EQ(c.poweroff_command, "doas poweroff");
    EXPECT_EQ(c.terminal, (Strings{"foot", "-e"}));
}

TEST(ConfigTest, a_terminal_with_quotes_is_split_like_an_exec)
{
    const Config c = parse_config(
            {{"a", "[commands]\nterminal = \"my term\" --x -e\n"}});

    EXPECT_EQ(c.terminal, (Strings{"my term", "--x", "-e"}));
}

TEST(ConfigTest, a_later_source_wins_key_by_key)
{
    const Config c = parse_config(
            {{"global", "[theme]\nscale = 2\n[commands]\nreboot = a\n"},
             {"user", "[commands]\nreboot = b\npoweroff = c\n"}});

    EXPECT_EQ(c.scale, 2.0);
    EXPECT_EQ(c.reboot_command, "b");
    EXPECT_EQ(c.poweroff_command, "c");
}

TEST(ConfigTest, a_wrong_value_keeps_the_value_of_the_lower_source)
{
    for (const char* bad : {"", "abc", "0", "-2", "2x", "1000", "nan"}) {
        Strings problems;
        const Config c = parse_config(
                {{"global", "[theme]\nscale = 2\n"},
                 {"user", std::string("[theme]\nscale = ") + bad + "\n"}},
                &problems);

        EXPECT_EQ(c.scale, 2.0) << bad;
        ASSERT_EQ(problems.size(), 1u) << bad;
        EXPECT_NE(problems[0].find("scale"), std::string::npos);
    }
}

TEST(ConfigTest, an_empty_command_is_a_problem_and_keeps_the_default)
{
    Strings problems;
    const Config c = parse_config(
            {{"a", "[commands]\nreboot =\npoweroff =\nterminal =\n"}},
            &problems);

    EXPECT_EQ(c.reboot_command, "systemctl reboot");
    EXPECT_EQ(c.poweroff_command, "systemctl poweroff");
    EXPECT_EQ(c.terminal, (Strings{"xterm", "-e"}));
    EXPECT_EQ(problems.size(), 3u);
}

TEST(ConfigTest, unknown_keys_and_sections_are_ignored)
{
    Strings problems;
    const Config c = parse_config(
            {{"a", "[plugins]\nclock = on\n[theme]\nfont = Sans\nscale = 3\n"}},
            &problems);

    EXPECT_EQ(c.scale, 3.0);
    EXPECT_TRUE(problems.empty());
}

TEST(ConfigTest, a_wrong_line_is_told_with_the_name_of_the_file)
{
    Strings problems;
    parse_config({{"/x/config", "[theme]\nnonsense\n"}}, &problems);

    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems[0].rfind("/x/config:2: ", 0), 0u);
}

TEST(ConfigTest, problems_may_be_ignored)
{
    EXPECT_EQ(parse_config({{"a", "nonsense\n[theme]\nscale = x\n"}}).scale,
              1.0);
}

TEST(ConfigTest, the_files_go_from_the_lowest_priority_to_the_user)
{
    EXPECT_EQ(tiny::wm::config_files("/h/.config", "/etc/xdg:/usr/etc", "/h"),
              (Strings{"/usr/etc/tiny-shell.ini", "/etc/xdg/tiny-shell.ini",
                       "/h/.config/tiny-shell.ini"}));
    EXPECT_EQ(
            tiny::wm::config_files(nullptr, nullptr, "/h"),
            (Strings{"/etc/xdg/tiny-shell.ini", "/h/.config/tiny-shell.ini"}));
}

class ConfigFilesTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        char path[] = "/tmp/tiny-config-XXXXXX";
        ASSERT_NE(mkdtemp(path), nullptr);
        dir = path;
    }

    void TearDown() override
    {
        for (const std::string& file : written) {
            unlink(file.c_str());
        }
        rmdir(dir.c_str());
    }

    std::string write(const std::string& name, const std::string& text)
    {
        const std::string file = dir + "/" + name;
        std::ofstream(file) << text;
        written.push_back(file);
        return file;
    }

    std::string dir;
    Strings written;
};

TEST_F(ConfigFilesTest, load_layers_the_files_and_skips_the_missing_ones)
{
    const std::string first = write("global", "[theme]\nscale = 2\n");
    const std::string second = write("user", "[commands]\nreboot = r\n");

    Strings problems;
    const Config c =
            tiny::wm::load_config({first, dir + "/missing", second}, &problems);

    EXPECT_EQ(c.scale, 2.0);
    EXPECT_EQ(c.reboot_command, "r");
    EXPECT_TRUE(problems.empty());
}

TEST_F(ConfigFilesTest, load_names_the_file_of_a_problem)
{
    const std::string file = write("config", "oops\n");

    Strings problems;
    tiny::wm::load_config({file}, &problems);

    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems[0].rfind(file + ":1: ", 0), 0u);
}

TEST_F(ConfigFilesTest, no_file_means_the_defaults)
{
    EXPECT_EQ(tiny::wm::load_config({dir + "/missing"}).scale, 1.0);
}

TEST(ConfigTest,
     the_favorites_are_a_list_without_blanks_empty_items_and_repeats)
{
    const Config c = parse_config(
            {{"a", "[dock]\nfavorites = a.desktop; b.desktop;;a.desktop ;\n"}});

    EXPECT_EQ(c.favorites, (Strings{"a.desktop", "b.desktop"}));
}

TEST(ConfigTest, the_favorites_of_the_user_replace_the_global_ones)
{
    const Config c = parse_config({{"global", "[dock]\nfavorites = a;b\n"},
                                   {"user", "[dock]\nfavorites =\n"}});

    EXPECT_TRUE(c.favorites.empty());
    EXPECT_EQ(parse_config({{"global", "[dock]\nfavorites = a;b\n"}}).favorites,
              (Strings{"a", "b"}));
}

TEST(ConfigTest, the_file_of_the_user_is_in_the_configuration_home)
{
    EXPECT_EQ(tiny::wm::user_config_file("/c", "/h"), "/c/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file(nullptr, "/h"),
              "/h/.config/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file("relative", "/h"),
              "/h/.config/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file("/", nullptr), "/tiny-shell.ini");
    EXPECT_EQ(tiny::wm::user_config_file(nullptr, nullptr), "");
    EXPECT_EQ(tiny::wm::user_config_file("", ""), "");
}

TEST_F(ConfigFilesTest, save_favorites_makes_the_file)
{
    const std::string file = dir + "/tiny-shell.ini";
    written.push_back(file);

    std::string error;
    ASSERT_TRUE(
            tiny::wm::save_favorites(file, {"a.desktop", "b.desktop"}, &error))
            << error;

    EXPECT_EQ(tiny::wm::load_config({file}).favorites,
              (Strings{"a.desktop", "b.desktop"}));
    EXPECT_NE(access((file + ".tmp").c_str(), F_OK), 0);
}

TEST_F(ConfigFilesTest, save_favorites_keeps_the_values_but_not_the_comments)
{
    const std::string file =
            write("tiny-shell.ini", "# my notes\n[theme]\nscale = 2\n"
                                    "[dock]\nfavorites = old.desktop\n"
                                    "[future]\nkey = value\n");

    ASSERT_TRUE(tiny::wm::save_favorites(file, {"new.desktop"}));

    const Config c = tiny::wm::load_config({file});
    EXPECT_EQ(c.scale, 2.0);
    EXPECT_EQ(c.favorites, (Strings{"new.desktop"}));
    std::ifstream in(file);
    const std::string text((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    EXPECT_EQ(text.find("my notes"), std::string::npos);
    EXPECT_NE(text.find("[future]\nkey = value\n"), std::string::npos);
}

TEST_F(ConfigFilesTest, save_favorites_writes_an_empty_list)
{
    const std::string file = write("tiny-shell.ini", "[dock]\nfavorites = a\n");

    ASSERT_TRUE(tiny::wm::save_favorites(file, {}));

    EXPECT_TRUE(tiny::wm::load_config({file}).favorites.empty());
}

TEST_F(ConfigFilesTest, save_favorites_makes_the_directory_of_the_file)
{
    const std::string file = dir + "/config/tiny-shell.ini";

    ASSERT_TRUE(tiny::wm::save_favorites(file, {"a"}));

    EXPECT_EQ(tiny::wm::load_config({file}).favorites, (Strings{"a"}));
    unlink(file.c_str());
    rmdir((dir + "/config").c_str());
}

TEST_F(ConfigFilesTest, save_favorites_refuses_an_id_that_breaks_the_file)
{
    const std::string file = write("tiny-shell.ini", "[dock]\nfavorites = a\n");

    for (const char* id : {"", "a;b", "a\nb", "a\rb"}) {
        std::string error;
        EXPECT_FALSE(tiny::wm::save_favorites(file, {id}, &error)) << id;
        EXPECT_FALSE(error.empty());
    }
    EXPECT_EQ(tiny::wm::load_config({file}).favorites, (Strings{"a"}));
}

TEST_F(ConfigFilesTest, save_favorites_tells_when_the_file_cannot_be_written)
{
    std::string error;

    EXPECT_FALSE(tiny::wm::save_favorites(dir + "/missing/x/tiny-shell.ini",
                                          {"a"}, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(
            tiny::wm::save_favorites(dir + "/missing/x/tiny-shell.ini", {"a"}));
}

TEST_F(ConfigFilesTest,
       save_favorites_keeps_the_old_file_when_it_cannot_replace)
{
    // a directory cannot be replaced by a file
    const std::string file = dir + "/tiny-shell.ini";
    ASSERT_EQ(mkdir(file.c_str(), 0777), 0);

    std::string error;
    EXPECT_FALSE(tiny::wm::save_favorites(file, {"a"}, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(access((file + ".tmp").c_str(), F_OK), 0);
    rmdir(file.c_str());
}

TEST_F(ConfigFilesTest, save_favorites_tells_when_the_directory_is_read_only)
{
    if (geteuid() == 0) {
        GTEST_SKIP() << "root can write anywhere";
    }
    const std::string readonly = dir + "/readonly";
    ASSERT_EQ(mkdir(readonly.c_str(), 0500), 0);

    std::string error;
    EXPECT_FALSE(tiny::wm::save_favorites(readonly + "/tiny-shell.ini", {"a"},
                                          &error));
    EXPECT_FALSE(error.empty());
    rmdir(readonly.c_str());
}

TEST_F(ConfigFilesTest, save_favorites_tells_when_the_disk_is_full)
{
    const std::string file = dir + "/tiny-shell.ini";
    // a write over the limit of the size of a file fails (EFBIG)
    struct rlimit old_limit;
    ASSERT_EQ(getrlimit(RLIMIT_FSIZE, &old_limit), 0);
    struct rlimit tiny_limit = old_limit;
    tiny_limit.rlim_cur = 1;
    void (*old_handler)(int) = signal(SIGXFSZ, SIG_IGN);
    ASSERT_EQ(setrlimit(RLIMIT_FSIZE, &tiny_limit), 0);

    std::string error;
    const bool saved = tiny::wm::save_favorites(file, {"a.desktop"}, &error);

    setrlimit(RLIMIT_FSIZE, &old_limit);
    signal(SIGXFSZ, old_handler);
    EXPECT_FALSE(saved);
    EXPECT_FALSE(error.empty());
    EXPECT_NE(access(file.c_str(), F_OK), 0);
    EXPECT_NE(access((file + ".tmp").c_str(), F_OK), 0);
}

TEST_F(ConfigFilesTest, save_favorites_writes_through_a_symbolic_link)
{
    const std::string real = write("real.ini", "[theme]\nscale = 2\n");
    const std::string link = dir + "/link.ini";
    ASSERT_EQ(symlink(real.c_str(), link.c_str()), 0);
    written.push_back(link);

    ASSERT_TRUE(tiny::wm::save_favorites(link, {"a.desktop"}));

    struct stat info;
    ASSERT_EQ(lstat(link.c_str(), &info), 0);
    EXPECT_TRUE(S_ISLNK(info.st_mode));
    const Config c = tiny::wm::load_config({real});
    EXPECT_EQ(c.favorites, (Strings{"a.desktop"}));
    EXPECT_EQ(c.scale, 2.0);
}

TEST_F(ConfigFilesTest, save_favorites_keeps_the_permissions_of_the_file)
{
    const std::string file = write("tiny-shell.ini", "[dock]\nfavorites = a\n");
    ASSERT_EQ(chmod(file.c_str(), 0600), 0);

    ASSERT_TRUE(tiny::wm::save_favorites(file, {"b"}));

    struct stat info;
    ASSERT_EQ(stat(file.c_str(), &info), 0);
    EXPECT_EQ(info.st_mode & 07777, 0600u);
}

TEST_F(ConfigFilesTest, save_values_sets_the_values_and_keeps_the_others)
{
    const std::string file = write(
            "tiny-shell.ini", "[theme]\nscale = 2\n[dock]\nfavorites = a\n");

    std::string error;
    ASSERT_TRUE(tiny::wm::save_values(file,
                                      {{"theme", "font", "Noto Sans"},
                                       {"theme", "scale", "1.5"},
                                       {"commands", "terminal", "foot -e"}},
                                      &error))
            << error;

    const Config c = tiny::wm::load_config({file});
    EXPECT_EQ(c.font, "Noto Sans");
    EXPECT_EQ(c.scale, 1.5);
    EXPECT_EQ(c.terminal_line, "foot -e");
    EXPECT_EQ(c.favorites, (Strings{"a"}));
}

TEST_F(ConfigFilesTest, save_values_refuses_what_would_break_the_file)
{
    const std::string file = write("tiny-shell.ini", "[theme]\nscale = 2\n");
    const std::vector<tiny::wm::IniValue> wrong[] = {
            {{"theme", "font", "a\nb"}},
            {{"theme", "font", "a\rb"}},
            {{"theme", "", "x"}},
            {{"theme", "a=b", "x"}},
            {{"theme", "a\nb", "x"}},
            {{"the]me", "a", "x"}},
            {{"the\nme", "a", "x"}},
            {{"theme", "scale", "3"}, {"theme", "font", "a\nb"}}};

    for (const auto& values : wrong) {
        std::string error;
        EXPECT_FALSE(tiny::wm::save_values(file, values, &error));
        EXPECT_NE(error.find("wrong setting"), std::string::npos) << error;
    }
    EXPECT_FALSE(tiny::wm::save_values(file, wrong[0])); // no error wanted
    // nothing was written, not even the part that was right
    EXPECT_EQ(tiny::wm::load_config({file}).scale, 2.0);
}

TEST(ConfigTest, reads_the_size_and_the_style_of_the_font)
{
    const Config c = parse_config(
            {{"a", "[theme]\nfont_size = 10.5\nfont_style = Bold Italic\n"}});

    EXPECT_EQ(c.font_size, 10.5);
    EXPECT_EQ(c.font_style, "Bold Italic");
    EXPECT_EQ(parse_config({}).font_size, 12.0);
    EXPECT_TRUE(parse_config({}).font_style.empty());
}

TEST(ConfigTest, a_wrong_size_keeps_the_default_and_an_empty_style_is_right)
{
    Strings problems;

    const Config c = parse_config(
            {{"a", "[theme]\nfont_size = huge\nfont_style =\n"}}, &problems);

    EXPECT_EQ(c.font_size, 12.0);
    EXPECT_TRUE(c.font_style.empty());
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("font_size"), std::string::npos);
}

TEST(ConfigTest, the_size_of_the_font_is_a_number_from_4_to_96)
{
    using tiny::wm::parse_font_size;

    EXPECT_EQ(parse_font_size("12"), 12.0);
    EXPECT_EQ(parse_font_size(" 4 "), 4.0);
    EXPECT_EQ(parse_font_size("96"), 96.0);
    EXPECT_EQ(parse_font_size("9.5"), 9.5);
    EXPECT_FALSE(parse_font_size(""));
    EXPECT_FALSE(parse_font_size("3.9"));
    EXPECT_FALSE(parse_font_size("96.1"));
    EXPECT_FALSE(parse_font_size("12pt"));
    EXPECT_FALSE(parse_font_size("nan"));
}

TEST(ConfigTest, a_style_is_a_name_without_control_characters)
{
    using tiny::wm::valid_font_style;

    EXPECT_TRUE(valid_font_style(""));
    EXPECT_TRUE(valid_font_style("Bold Italic"));
    EXPECT_FALSE(valid_font_style("Bold\nItalic"));
}

TEST(ConfigTest, reads_the_rendering_of_text)
{
    const Config c = parse_config(
            {{"a", "[xft]\nantialias = no\nhinting = slight\nrgba = vrgb\n"}});

    EXPECT_EQ(c.xft_antialias, "no");
    EXPECT_EQ(c.xft_hinting, "slight");
    EXPECT_EQ(c.xft_rgba, "vrgb");
    EXPECT_TRUE(parse_config({}).xft_antialias.empty());
}

TEST(ConfigTest, an_empty_rendering_value_is_not_set_and_a_wrong_one_is_told)
{
    Strings problems;
    const Config c = parse_config(
            {{"a", "[xft]\nantialias = yes\nhinting =\nrgba = rgb\n"},
             {"b", "[xft]\nantialias = maybe\nhinting = sharp\n"}},
            &problems);

    EXPECT_EQ(c.xft_antialias, "yes"); // the wrong value of "b" is left out
    EXPECT_TRUE(c.xft_hinting.empty());
    EXPECT_EQ(c.xft_rgba, "rgb");
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].find("b: [xft] antialias is not one of"),
              std::string::npos);
    EXPECT_NE(problems[1].find("[xft] hinting is not one of"),
              std::string::npos);
}

TEST(ConfigTest, the_choices_of_the_rendering_start_with_the_default)
{
    using tiny::wm::valid_xft;
    using tiny::wm::xft_choices;
    using tiny::wm::xft_key;
    using tiny::wm::XftOption;

    for (const auto option :
         {XftOption::Antialias, XftOption::Hinting, XftOption::Rgba}) {
        ASSERT_GT(xft_choices(option).size(), 2u);
        EXPECT_STREQ(xft_choices(option)[0].value, "");
        EXPECT_TRUE(valid_xft(option, ""));
        EXPECT_FALSE(valid_xft(option, "bogus"));
    }
    EXPECT_STREQ(xft_key(XftOption::Antialias), "antialias");
    EXPECT_STREQ(xft_key(XftOption::Hinting), "hinting");
    EXPECT_STREQ(xft_key(XftOption::Rgba), "rgba");
    EXPECT_TRUE(valid_xft(XftOption::Hinting, "full"));
}

TEST(ConfigTest, reads_the_format_of_the_clock)
{
    EXPECT_EQ(parse_config({}).clock_format, "%A %e %B %R");
    EXPECT_EQ(parse_config({{"a", "[panel]\nclock_format = %H:%M\n"}})
                      .clock_format,
              "%H:%M");
    // empty: no clock
    EXPECT_TRUE(parse_config({{"a", "[panel]\nclock_format =\n"}})
                        .clock_format.empty());
}

TEST(ConfigTest, the_programs_are_told_the_look_unless_it_is_turned_off)
{
    EXPECT_TRUE(parse_config({}).xsettings);
    EXPECT_TRUE(parse_config({{"a", "[theme]\nxsettings = yes\n"}}).xsettings);
    EXPECT_FALSE(parse_config({{"a", "[theme]\nxsettings = no\n"}}).xsettings);
}

TEST(ConfigTest, a_wrong_value_of_xsettings_is_refused)
{
    std::vector<std::string> problems;

    const auto c =
            parse_config({{"a", "[theme]\nxsettings = maybe\n"}}, &problems);

    EXPECT_TRUE(c.xsettings);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("a: [theme] xsettings must be"),
              std::string::npos);
}

TEST(ConfigTest, the_system_tray_is_on_unless_it_is_turned_off)
{
    EXPECT_TRUE(parse_config({}).system_tray);
    EXPECT_TRUE(
            parse_config({{"a", "[panel]\nsystem_tray = yes\n"}}).system_tray);
    EXPECT_FALSE(
            parse_config({{"a", "[panel]\nsystem_tray = no\n"}}).system_tray);
}

TEST(ConfigTest, a_wrong_value_of_the_system_tray_is_refused)
{
    std::vector<std::string> problems;
    const Config c =
            parse_config({{"a", "[panel]\nsystem_tray = maybe\n"}}, &problems);

    EXPECT_TRUE(c.system_tray);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("a: [panel] system_tray must be"),
              std::string::npos);
}

TEST(ConfigTest, the_session_starts_the_autostart_unless_it_is_turned_off)
{
    EXPECT_TRUE(parse_config({}).session_autostart);
    EXPECT_TRUE(parse_config({{"a", "[session]\nautostart = yes\n"}})
                        .session_autostart);
    EXPECT_FALSE(parse_config({{"a", "[session]\nautostart = no\n"}})
                         .session_autostart);
}

TEST(ConfigTest, a_wrong_value_of_the_autostart_is_refused)
{
    std::vector<std::string> problems;
    const Config c =
            parse_config({{"a", "[session]\nautostart = maybe\n"}}, &problems);

    EXPECT_TRUE(c.session_autostart);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("a: [session] autostart must be"),
              std::string::npos);
}

TEST(ConfigTest, the_end_of_the_session_is_confirmed_only_when_it_is_turned_on)
{
    EXPECT_FALSE(parse_config({}).session_confirm);
    EXPECT_TRUE(parse_config({{"a", "[session]\nconfirm = yes\n"}})
                        .session_confirm);
    EXPECT_FALSE(
            parse_config({{"a", "[session]\nconfirm = no\n"}}).session_confirm);
}

TEST(ConfigTest, a_wrong_value_of_the_confirmation_is_refused)
{
    std::vector<std::string> problems;
    const Config c =
            parse_config({{"a", "[session]\nconfirm = sure\n"}}, &problems);

    EXPECT_FALSE(c.session_confirm);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("a: [session] confirm must be"),
              std::string::npos);
}

TEST(ConfigTest, the_keyboard_is_left_alone_unless_a_layout_is_set)
{
    const Config none = parse_config({});
    EXPECT_EQ(none.keyboard_layout, "");
    EXPECT_EQ(none.keyboard_variant, "");
    EXPECT_EQ(none.keyboard_options, "");

    const Config c = parse_config(
            {{"a", "[keyboard]\nlayout = cz,us\nvariant = ,qwerty\n"
                   "options = grp:alt_shift_toggle\n"}});
    EXPECT_EQ(c.keyboard_layout, "cz,us");
    EXPECT_EQ(c.keyboard_variant, ",qwerty");
    EXPECT_EQ(c.keyboard_options, "grp:alt_shift_toggle");
}

TEST(ConfigTest, more_than_four_layouts_are_refused)
{
    std::vector<std::string> problems;
    const Config c = parse_config({{"a", "[keyboard]\nlayout = a,b,c,d,e\n"}},
                                  &problems);

    EXPECT_EQ(c.keyboard_layout, "");
    EXPECT_EQ(problems.size(), 1u);
    EXPECT_EQ(parse_config({{"a", "[keyboard]\nlayout = a,b,c,d\n"}})
                      .keyboard_layout,
              "a,b,c,d");
}

TEST(ConfigTest, the_layout_is_in_the_panel_only_when_it_is_turned_on)
{
    EXPECT_FALSE(parse_config({}).keyboard_indicator);
    EXPECT_TRUE(parse_config({{"a", "[keyboard]\nindicator = yes\n"}})
                        .keyboard_indicator);
    EXPECT_FALSE(parse_config({{"a", "[keyboard]\nindicator = no\n"}})
                         .keyboard_indicator);

    std::vector<std::string> problems;
    const Config c =
            parse_config({{"a", "[keyboard]\nindicator = maybe\n"}}, &problems);
    EXPECT_FALSE(c.keyboard_indicator);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("a: [keyboard] indicator must be"),
              std::string::npos);
}

TEST(ConfigTest, the_options_of_the_keyboard_can_be_many)
{
    std::vector<std::string> problems;
    const Config c = parse_config(
            {{"a", "[keyboard]\noptions = grp:caps_toggle,caps:escape,"
                   "compose:ralt,ctrl:nocaps,terminate:ctrl_alt_bksp\n"}},
            &problems);

    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(c.keyboard_options,
              "grp:caps_toggle,caps:escape,compose:ralt,ctrl:nocaps,"
              "terminate:ctrl_alt_bksp");
}

TEST(ConfigTest, a_wrong_name_of_the_keyboard_is_refused)
{
    std::vector<std::string> problems;
    const Config c =
            parse_config({{"a", "[keyboard]\nlayout = cz us\nvariant = -print\n"
                                "options = grp:toggle\n"}},
                         &problems);

    EXPECT_EQ(c.keyboard_layout, "");
    EXPECT_EQ(c.keyboard_variant, "");
    EXPECT_EQ(c.keyboard_options, "grp:toggle");
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].find("a: [keyboard] layout must be"),
              std::string::npos);
    EXPECT_NE(problems[1].find("a: [keyboard] variant must be"),
              std::string::npos);
}

TEST(ConfigTest, the_monitors_are_chosen_by_the_shell_unless_they_are_set)
{
    const Config c = parse_config({});

    EXPECT_EQ(c.monitor_primary, "auto");
    EXPECT_EQ(c.monitor_secondary, "auto");
    EXPECT_EQ(c.monitor_placement, tiny::wm::Placement::Keep);
}

TEST(ConfigTest, the_monitors_are_read)
{
    const Config c =
            parse_config({{"a", "[monitors]\nprimary = 2\n"
                                "secondary = HDMI-1\nplacement = Above\n"
                                "numbers = yes\n"}});

    EXPECT_EQ(c.monitor_primary, "2");
    EXPECT_EQ(c.monitor_secondary, "HDMI-1");
    EXPECT_EQ(c.monitor_placement, tiny::wm::Placement::Over);
}

TEST(ConfigTest, an_empty_monitor_is_auto)
{
    const Config c = parse_config({{"a", "[monitors]\nprimary =\n"}});

    EXPECT_EQ(c.monitor_primary, "auto");
}

TEST(ConfigTest, wrong_values_of_the_monitors_are_refused)
{
    std::vector<std::string> problems;
    const Config c = parse_config({{"a", "[monitors]\nprimary = two monitors\n"
                                         "placement = diagonal\n"}},
                                  &problems);

    EXPECT_EQ(c.monitor_primary, "auto");
    EXPECT_EQ(c.monitor_placement, tiny::wm::Placement::Keep);
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].find("[monitors] primary must be"),
              std::string::npos);
    EXPECT_NE(problems[1].find("[monitors] placement must be"),
              std::string::npos);
}

TEST(ConfigTest, a_format_of_the_clock_with_a_control_character_is_refused)
{
    std::vector<std::string> problems;
    const Config c = parse_config({{"a", "[panel]\nclock_format = %H\t%M\n"}},
                                  &problems);

    EXPECT_EQ(c.clock_format, "%A %e %B %R");
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("a: [panel] clock_format must be"),
              std::string::npos);
}

TEST(ConfigTest, reads_the_wallpaper)
{
    const Config c = parse_config(
            {{"a", "[wallpaper]\nfile = ~/Pictures/a b.jpg \nmode = tile\n"}});

    EXPECT_EQ(c.wallpaper_file, "~/Pictures/a b.jpg");
    EXPECT_EQ(c.wallpaper_mode, "tile");
    EXPECT_TRUE(parse_config({}).wallpaper_file.empty());
    EXPECT_TRUE(parse_config({}).wallpaper_mode.empty());
}

TEST(ConfigTest, a_wallpaper_that_is_set_is_told_even_when_it_is_empty)
{
    EXPECT_FALSE(parse_config({}).wallpaper_set);
    EXPECT_TRUE(parse_config({{"a", "[wallpaper]\nfile = /a.png\n"}})
                        .wallpaper_set);
    EXPECT_TRUE(parse_config({{"a", "[wallpaper]\nfile =\n"}}).wallpaper_set);
    EXPECT_FALSE(
            parse_config({{"a", "[wallpaper]\nmode = tile\n"}}).wallpaper_set);
}

TEST(ConfigTest, the_default_wallpaper_is_the_hazel_by_day_or_night)
{
    const std::filesystem::path root =
            std::filesystem::temp_directory_path() /
            ("tiny-wallpapers-" + std::to_string(getpid()));
    const std::filesystem::path dir = root / "backgrounds/tiny-shell";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "tiny-shell-hazel-day-1920x1080.jpg") << "x";
    std::ofstream(dir / "tiny-shell-hazel-night-1920x1080.jpg") << "x";
    const Strings dirs = {"/nonexistent", root.string()};

    EXPECT_EQ(tiny::wm::default_wallpaper(tiny::Variant::Light, dirs),
              (dir / "tiny-shell-hazel-day-1920x1080.jpg").string());
    EXPECT_EQ(tiny::wm::default_wallpaper(tiny::Variant::Dark, dirs),
              (dir / "tiny-shell-hazel-night-1920x1080.jpg").string());
    EXPECT_EQ(
            tiny::wm::default_wallpaper(tiny::Variant::Dark, {"/nonexistent"}),
            "");

    // the configuration wins, also with no wallpaper at all
    const Config unset = parse_config({});
    const Config set = parse_config({{"a", "[wallpaper]\nfile = /a.png\n"}});
    const Config none = parse_config({{"a", "[wallpaper]\nfile =\n"}});
    EXPECT_EQ(tiny::wm::wallpaper_of(unset, tiny::Variant::Dark, dirs),
              (dir / "tiny-shell-hazel-night-1920x1080.jpg").string());
    EXPECT_EQ(tiny::wm::wallpaper_of(set, tiny::Variant::Dark, dirs), "/a.png");
    EXPECT_EQ(tiny::wm::wallpaper_of(none, tiny::Variant::Dark, dirs), "");
    std::filesystem::remove_all(root);
}

TEST(ConfigTest, fill_is_the_default_mode_of_the_wallpaper)
{
    EXPECT_TRUE(parse_config({{"a", "[wallpaper]\nmode = fill\n"}})
                        .wallpaper_mode.empty());
    EXPECT_TRUE(parse_config({{"a", "[wallpaper]\nmode =\n"}})
                        .wallpaper_mode.empty());
}

TEST(ConfigTest,
     a_wrong_mode_of_the_wallpaper_is_told_and_the_other_value_stays)
{
    Strings problems;
    const Config c = parse_config({{"a", "[wallpaper]\nmode = center\n"},
                                   {"b", "[wallpaper]\nmode = stretch\n"}},
                                  &problems);

    EXPECT_EQ(c.wallpaper_mode, "center");
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("b: [wallpaper] mode must be"),
              std::string::npos);
}

TEST(ConfigTest, a_path_of_the_wallpaper_with_a_line_break_is_refused)
{
    // the ini reader has no line breaks in a value, so this is for the form
    EXPECT_TRUE(tiny::wm::valid_wallpaper_file(""));
    EXPECT_TRUE(tiny::wm::valid_wallpaper_file("/a/b c.png"));
    EXPECT_TRUE(tiny::wm::valid_wallpaper_file("~/a.png"));
    EXPECT_TRUE(tiny::wm::valid_wallpaper_file("~"));
    EXPECT_FALSE(tiny::wm::valid_wallpaper_file("pics/a.png"));
    EXPECT_FALSE(tiny::wm::valid_wallpaper_file("~other/a.png"));
    EXPECT_FALSE(tiny::wm::valid_wallpaper_file("/a\nb"));
    EXPECT_FALSE(tiny::wm::valid_wallpaper_file("/a\tb"));
}

TEST(ConfigTest, the_modes_of_the_wallpaper)
{
    using tiny::wm::valid_wallpaper_mode;
    using tiny::wm::wallpaper_choices;

    ASSERT_EQ(wallpaper_choices().size(), 3u);
    EXPECT_STREQ(wallpaper_choices()[0].value, "");
    EXPECT_STREQ(wallpaper_choices()[0].label, "Fill");
    EXPECT_TRUE(valid_wallpaper_mode(""));
    EXPECT_TRUE(valid_wallpaper_mode("fill"));
    EXPECT_TRUE(valid_wallpaper_mode("center"));
    EXPECT_TRUE(valid_wallpaper_mode("tile"));
    EXPECT_FALSE(valid_wallpaper_mode("stretch"));
}

TEST(ConfigTest, the_dock_hides_by_default)
{
    EXPECT_EQ(Config().dock_mode, tiny::wm::DockMode::AutoHide);
    EXPECT_EQ(parse_config({}).dock_mode, tiny::wm::DockMode::AutoHide);
}

TEST(ConfigTest, reads_the_mode_of_the_dock)
{
    EXPECT_EQ(parse_config({{"a", "[dock]\nmode = on\n"}}).dock_mode,
              tiny::wm::DockMode::On);
    EXPECT_EQ(parse_config({{"a", "[dock]\nmode = off\n"}}).dock_mode,
              tiny::wm::DockMode::Off);
    EXPECT_EQ(parse_config({{"a", "[dock]\nmode = on\n"},
                            {"b", "[dock]\nmode = autohide\n"}})
                      .dock_mode,
              tiny::wm::DockMode::AutoHide);
}

TEST(ConfigTest, a_wrong_mode_of_the_dock_is_a_problem_and_keeps_the_default)
{
    Strings problems;

    const Config c = parse_config({{"a", "[dock]\nmode = never\n"}}, &problems);

    EXPECT_EQ(c.dock_mode, tiny::wm::DockMode::AutoHide);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("mode"), std::string::npos);
}

TEST(ConfigTest, the_choices_of_the_dock_start_with_the_default)
{
    const tiny::wm::XftChoices choices = tiny::wm::dock_choices();

    ASSERT_EQ(choices.size(), 3u);
    EXPECT_STREQ(choices[0].value, "");
    EXPECT_STREQ(choices[1].value, "on");
    EXPECT_STREQ(choices[2].value, "off");
}

TEST(ConfigTest, the_choices_of_the_monitors_start_with_the_default)
{
    EXPECT_EQ(std::string(tiny::wm::placement_choices()[0].value), "");
    EXPECT_EQ(tiny::wm::placement_choices().size(), 5u);
    EXPECT_TRUE(tiny::wm::valid_placement_choice("above"));
    EXPECT_TRUE(tiny::wm::valid_placement_choice(""));
    EXPECT_FALSE(tiny::wm::valid_placement_choice("keep")); // "" is keep
}

TEST(ConfigTest, the_lock_has_its_defaults)
{
    const Config c = parse_config({});

    EXPECT_EQ(c.lock_command, "tiny-lock");
    EXPECT_EQ(c.lock_auth, "auto");
    EXPECT_EQ(c.lock_pam_service, "tiny-lock");
    EXPECT_EQ(c.lock_checker, "");
    EXPECT_EQ(c.lock_pwcheck, "");
}

TEST(ConfigTest, reads_the_lock)
{
    const Config c = parse_config(
            {{"a", "[commands]\nlock = my-lock --now\n"
                   "[lock]\nauth = pwcheck\npam_service = login\n"
                   "checker = /sbin/unix_chkpwd\npwcheck = /opt/pwcheck\n"}});

    EXPECT_EQ(c.lock_command, "my-lock --now");
    EXPECT_EQ(c.lock_auth, "pwcheck");
    EXPECT_EQ(c.lock_pam_service, "login");
    EXPECT_EQ(c.lock_checker, "/sbin/unix_chkpwd");
    EXPECT_EQ(c.lock_pwcheck, "/opt/pwcheck");
}

TEST(ConfigTest, a_wrong_value_of_the_lock_is_a_problem_and_keeps_the_default)
{
    Strings problems;
    const Config c = parse_config(
            {{"a", "[commands]\nlock =\n"
                   "[lock]\nauth = fingerprint\npam_service = my service\n"
                   "checker =\npwcheck =\n"}},
            &problems);

    EXPECT_EQ(c.lock_command, "tiny-lock");
    EXPECT_EQ(c.lock_auth, "auto");
    EXPECT_EQ(c.lock_pam_service, "tiny-lock");
    EXPECT_EQ(c.lock_checker, "");
    EXPECT_EQ(c.lock_pwcheck, "");
    EXPECT_EQ(problems.size(), 5u);
}

TEST(ConfigTest, a_pam_service_is_a_name_and_not_a_path)
{
    EXPECT_TRUE(tiny::wm::valid_pam_service("tiny-lock"));
    EXPECT_TRUE(tiny::wm::valid_pam_service("login"));
    EXPECT_FALSE(tiny::wm::valid_pam_service(""));
    EXPECT_FALSE(tiny::wm::valid_pam_service("my service"));
    EXPECT_FALSE(tiny::wm::valid_pam_service("../other"));
    EXPECT_FALSE(tiny::wm::valid_pam_service("a\nb"));
}

TEST(ConfigTest, a_program_path_is_a_text_without_control_characters)
{
    EXPECT_TRUE(tiny::wm::valid_program_path("/sbin/unix_chkpwd"));
    EXPECT_FALSE(tiny::wm::valid_program_path(""));
    EXPECT_FALSE(tiny::wm::valid_program_path("a\nb"));
}

TEST(ConfigTest, the_old_blank_after_is_not_read)
{
    Strings problems;
    parse_config({{"a", "[lock]\nblank_after = 5\n"}}, &problems);
    EXPECT_TRUE(problems.empty());
}

TEST(ConfigTest, the_screen_is_not_locked_by_default)
{
    EXPECT_EQ(parse_config({}).lock_delay_minutes, -1);
}

TEST(ConfigTest, delay_is_one_of_the_choices_or_never)
{
    for (const int minutes : {0, 1, 2, 5, 10, 30}) {
        const Config c = parse_config(
                {{"a", "[lock]\ndelay = " + std::to_string(minutes) + "\n"}});
        EXPECT_EQ(c.lock_delay_minutes, minutes);
    }
    EXPECT_EQ(
            parse_config({{"a", "[lock]\ndelay = never\n"}}).lock_delay_minutes,
            -1);
}

TEST(ConfigTest, another_delay_is_a_problem_and_keeps_the_default)
{
    for (const char* value : {"3", "60", "-1", "soon", ""}) {
        Strings problems;
        const Config c = parse_config(
                {{"a", std::string("[lock]\ndelay = ") + value + "\n"}},
                &problems);
        EXPECT_EQ(c.lock_delay_minutes, -1) << value;
        EXPECT_EQ(problems.size(), 1u) << value;
    }
}

TEST(ConfigTest, the_old_idle_after_is_not_read)
{
    const Config c = parse_config({{"a", "[lock]\nidle_after = 10\n"}});
    EXPECT_EQ(c.lock_delay_minutes, -1);
}

TEST(ConfigTest, the_form_offers_the_delays_of_the_lock)
{
    const tiny::wm::XftChoices choices = tiny::wm::lock_delay_choices();

    ASSERT_EQ(choices.size(), 7u);
    EXPECT_STREQ(choices[0].value, ""); // never, the default
    EXPECT_STREQ(choices[1].value, "0");
    EXPECT_STREQ(choices[6].value, "30");
    for (const auto& choice : choices) {
        EXPECT_TRUE(tiny::wm::valid_lock_delay_choice(choice.value));
    }
    EXPECT_TRUE(tiny::wm::valid_lock_delay_choice("never"));
    EXPECT_FALSE(tiny::wm::valid_lock_delay_choice("3"));
    EXPECT_FALSE(tiny::wm::valid_lock_delay_choice("60"));
}

TEST(ConfigTest, the_display_is_left_as_it_is_by_default)
{
    EXPECT_EQ(parse_config({}).display_off_minutes, -1);
}

TEST(ConfigTest, off_after_is_one_of_the_choices_or_never)
{
    for (const int minutes : {5, 10, 15, 30, 60}) {
        const Config c = parse_config(
                {{"a",
                  "[display]\noff_after = " + std::to_string(minutes) + "\n"}});
        EXPECT_EQ(c.display_off_minutes, minutes);
    }
    EXPECT_EQ(parse_config({{"a", "[display]\noff_after = never\n"}})
                      .display_off_minutes,
              0);
}

TEST(ConfigTest, an_empty_off_after_leaves_the_display_as_it_is)
{
    Strings problems;
    const Config c =
            parse_config({{"a", "[display]\noff_after =\n"}}, &problems);
    EXPECT_EQ(c.display_off_minutes, -1);
    EXPECT_TRUE(problems.empty());
}

TEST(ConfigTest, another_off_after_is_a_problem_and_keeps_the_default)
{
    for (const char* value : {"1", "7", "0", "-5", "soon"}) {
        Strings problems;
        const Config c = parse_config(
                {{"a", std::string("[display]\noff_after = ") + value + "\n"}},
                &problems);
        EXPECT_EQ(c.display_off_minutes, -1) << value;
        EXPECT_EQ(problems.size(), 1u) << value;
    }
}

TEST(ConfigTest, the_form_offers_the_times_of_the_display_power_saving)
{
    const tiny::wm::XftChoices choices = tiny::wm::display_off_choices();

    ASSERT_EQ(choices.size(), 7u);
    EXPECT_STREQ(choices[0].value, ""); // unchanged, the default
    EXPECT_STREQ(choices[1].value, "never");
    EXPECT_STREQ(choices[6].value, "60");
    for (const auto& choice : choices) {
        EXPECT_TRUE(tiny::wm::valid_display_off_choice(choice.value));
    }
    EXPECT_FALSE(tiny::wm::valid_display_off_choice("1"));
    EXPECT_FALSE(tiny::wm::valid_display_off_choice("0"));
}

TEST(ConfigTest, dpms_is_used_by_default_and_can_be_switched_off)
{
    EXPECT_TRUE(parse_config({}).display_dpms);
    EXPECT_FALSE(parse_config({{"a", "[display]\ndpms = no\n"}}).display_dpms);
    EXPECT_TRUE(parse_config({{"a", "[display]\ndpms = yes\n"}}).display_dpms);

    Strings problems;
    const Config c =
            parse_config({{"a", "[display]\ndpms = maybe\n"}}, &problems);
    EXPECT_TRUE(c.display_dpms);
    EXPECT_EQ(problems.size(), 1u);
}

TEST(ConfigTest, the_form_offers_yes_and_no_for_dpms)
{
    const tiny::wm::XftChoices choices = tiny::wm::dpms_choices();
    ASSERT_EQ(choices.size(), 2u);
    EXPECT_STREQ(choices[0].value, "yes");
    EXPECT_STREQ(choices[1].value, "no");
    EXPECT_TRUE(tiny::wm::valid_dpms_choice("no"));
    EXPECT_FALSE(tiny::wm::valid_dpms_choice(""));
}

TEST(ConfigTest, the_panel_and_the_dock_follow_the_theme_by_default)
{
    EXPECT_EQ(Config().panel_style, tiny::Look::Auto);
    EXPECT_EQ(Config().dock_style, tiny::Look::Auto);
    EXPECT_EQ(parse_config({}).panel_style, tiny::Look::Auto);
    EXPECT_EQ(parse_config({}).dock_style, tiny::Look::Auto);
}

TEST(ConfigTest, reads_the_style_of_the_panel_and_of_the_dock)
{
    const Config c = parse_config(
            {{"a", "[panel]\nstyle = Light\n[dock]\nstyle = dark\n"}});

    EXPECT_EQ(c.panel_style, tiny::Look::Light);
    EXPECT_EQ(c.dock_style, tiny::Look::Dark);
    EXPECT_EQ(c.style, tiny::Variant::Dark) << "the theme is not changed";
}

TEST(ConfigTest, the_lock_screen_has_a_style_of_its_own)
{
    EXPECT_EQ(Config().lock_style, tiny::Look::Auto);
    const Config c = parse_config({{"a", "[lock]\nstyle = dark\n"}});
    EXPECT_EQ(c.lock_style, tiny::Look::Dark);
    EXPECT_EQ(c.style, tiny::Variant::Dark);

    std::vector<std::string> problems;
    parse_config({{"a", "[lock]\nstyle = blue\n"}}, &problems);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("[lock] style must be auto, light or dark"),
              std::string::npos);
}

TEST(ConfigTest, the_launcher_has_a_style_of_its_own)
{
    EXPECT_EQ(Config().launcher_style, tiny::Look::Auto);
    const Config c = parse_config({{"a", "[launcher]\nstyle = light\n"}});
    EXPECT_EQ(c.launcher_style, tiny::Look::Light);
}

TEST(ConfigTest, a_wrong_style_of_the_panel_or_the_dock_is_a_problem)
{
    std::vector<std::string> problems;
    const Config c = parse_config(
            {{"a", "[panel]\nstyle = blue\n[dock]\nstyle = \n"}}, &problems);

    EXPECT_EQ(c.panel_style, tiny::Look::Auto);
    EXPECT_EQ(c.dock_style, tiny::Look::Auto);
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].find("[panel] style must be auto, light or dark"),
              std::string::npos);
    EXPECT_NE(problems[1].find("[dock] style must be auto, light or dark"),
              std::string::npos);
}

TEST(ConfigTest, the_choices_of_the_styles_have_the_values_of_the_file)
{
    using tiny::wm::look_choices;
    using tiny::wm::style_choices;
    EXPECT_EQ(look_choices().size(), 3u);
    EXPECT_TRUE(tiny::wm::valid_look_choice(""));
    EXPECT_TRUE(tiny::wm::valid_look_choice("light"));
    EXPECT_TRUE(tiny::wm::valid_look_choice("dark"));
    EXPECT_FALSE(tiny::wm::valid_look_choice("auto"));
    EXPECT_EQ(style_choices().size(), 2u);
    EXPECT_TRUE(tiny::wm::valid_style_choice(""));
    EXPECT_TRUE(tiny::wm::valid_style_choice("light"));
    EXPECT_FALSE(tiny::wm::valid_style_choice("dark"));
}
