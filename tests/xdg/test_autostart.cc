#include "helpers.h"

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "xdg/autostart.h"

namespace fs = std::filesystem;

namespace {

class AutostartTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-autostart" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "user" / "autostart");
        fs::create_directories(root / "system" / "autostart");
        fs::create_directories(root / "bin");
        std::ofstream(root / "bin" / "present") << "#!/bin/sh\n";
        fs::permissions(root / "bin" / "present", fs::perms::owner_all);
    }

    void TearDown() override { fs::remove_all(root); }

    void write(const char* where, const std::string& name,
               const std::string& extra = "",
               const std::string& exec = "Exec=program\n")
    {
        std::ofstream(root / where / "autostart" / name)
                << "[Desktop Entry]\nType=Application\nName=" << name << "\n"
                << exec << extra;
    }

    std::vector<std::string> dirs() const
    {
        return {(root / "user").string(), (root / "system").string()};
    }

    std::vector<std::string> ids(std::string_view desktops = "tiny-shell") const
    {
        std::vector<std::string> out;
        for (const auto& app : tiny::xdg::autostart_applications(
                     dirs(), desktops, (root / "bin").string())) {
            out.push_back(app.id);
        }
        return out;
    }

    fs::path root;
};

} // namespace

TEST_F(AutostartTest, the_files_of_every_directory_are_found_by_name)
{
    write("system", "b.desktop");
    write("user", "c.desktop");
    write("system", "a.desktop");

    EXPECT_EQ(ids(), (std::vector<std::string>{"a.desktop", "b.desktop",
                                               "c.desktop"}));
}

TEST_F(AutostartTest, the_application_knows_its_file_and_its_command)
{
    write("user", "a.desktop", "", "Exec=program --now %u\n");

    const auto apps = tiny::xdg::autostart_applications(dirs(), "", "");

    ASSERT_EQ(apps.size(), 1u);
    EXPECT_EQ(apps[0].path, (root / "user" / "autostart" / "a.desktop"));
    EXPECT_EQ(apps[0].entry.exec, "program --now %u");
}

TEST_F(AutostartTest, a_file_of_the_user_replaces_the_one_of_the_system)
{
    write("system", "a.desktop", "", "Exec=system\n");
    write("user", "a.desktop", "", "Exec=user\n");

    const auto apps = tiny::xdg::autostart_applications(dirs(), "", "");

    ASSERT_EQ(apps.size(), 1u);
    EXPECT_EQ(apps[0].entry.exec, "user");
}

TEST_F(AutostartTest, a_hidden_file_of_the_user_turns_off_the_one_of_the_system)
{
    write("system", "a.desktop");
    write("user", "a.desktop", "Hidden=true\n");

    EXPECT_TRUE(ids().empty());
}

TEST_F(AutostartTest, the_switch_of_gnome_turns_a_file_off)
{
    write("user", "a.desktop", "X-GNOME-Autostart-enabled=false\n");
    write("user", "b.desktop", "X-GNOME-Autostart-enabled=true\n");

    EXPECT_EQ(ids(), std::vector<std::string>{"b.desktop"});
}

TEST_F(AutostartTest, a_file_with_a_condition_is_not_started)
{
    // the screen reader of GNOME: a setting says whether it runs
    write("user", "orca.desktop",
          "AutostartCondition=GSettings org.gnome.desktop.a11y.applications "
          "screen-reader-enabled\n");
    write("user", "plain.desktop");

    EXPECT_EQ(ids(), std::vector<std::string>{"plain.desktop"});
}

TEST_F(AutostartTest, a_file_that_can_not_be_read_hides_nothing)
{
    write("system", "a.desktop");
    fs::create_directories(root / "user" / "autostart" / "a.desktop");

    EXPECT_EQ(ids(), std::vector<std::string>{"a.desktop"});
}

TEST_F(AutostartTest, the_desktop_decides_where_the_file_is_started)
{
    write("user", "only.desktop", "OnlyShowIn=GNOME;tiny-shell;\n");
    write("user", "other.desktop", "OnlyShowIn=GNOME;\n");
    write("user", "not.desktop", "NotShowIn=tiny-shell;\n");
    write("user", "all.desktop");

    EXPECT_EQ(ids("tiny-shell"),
              (std::vector<std::string>{"all.desktop", "only.desktop"}));
    EXPECT_EQ(ids("GNOME"),
              (std::vector<std::string>{"all.desktop", "not.desktop",
                                        "only.desktop", "other.desktop"}));
}

TEST_F(AutostartTest, a_program_that_is_not_installed_is_not_started)
{
    write("user", "gone.desktop", "TryExec=missing\n");
    write("user", "here.desktop", "TryExec=present\n");

    EXPECT_EQ(ids(), std::vector<std::string>{"here.desktop"});
}

TEST_F(AutostartTest, what_is_not_an_application_is_left_out)
{
    write("user", "dir.desktop", "");
    std::ofstream(root / "user" / "autostart" / "link.desktop")
            << "[Desktop Entry]\nType=Link\nName=L\nURL=x\n";
    std::ofstream(root / "user" / "autostart" / "noexec.desktop")
            << "[Desktop Entry]\nType=Application\nName=E\n";
    std::ofstream(root / "user" / "autostart" / "readme.txt") << "text\n";

    EXPECT_EQ(ids(), std::vector<std::string>{"dir.desktop"});
}

TEST_F(AutostartTest, an_application_that_is_not_in_a_menu_is_started)
{
    write("user", "a.desktop", "NoDisplay=true\n");

    EXPECT_EQ(ids(), std::vector<std::string>{"a.desktop"});
}

TEST_F(AutostartTest, a_missing_directory_is_no_files)
{
    EXPECT_TRUE(tiny::xdg::autostart_applications({"/nonexistent"}, "", "")
                        .empty());
}

namespace {

using tiny::xdg::AutostartItem;

class AutostartItemsTest : public AutostartTest {
  protected:
    std::vector<AutostartItem> items() const
    {
        return tiny::xdg::autostart_items(dirs(), "tiny-shell",
                                          (root / "bin").string());
    }

    const AutostartItem& item(const std::vector<AutostartItem>& all,
                              const std::string& id) const
    {
        const auto found = std::find_if(
                all.begin(), all.end(),
                [&](const AutostartItem& i) { return i.app.id == id; });
        EXPECT_NE(found, all.end()) << id;
        return *found;
    }

    std::string read(const char* where, const std::string& name) const
    {
        std::ifstream in(root / where / "autostart" / name);
        return {std::istreambuf_iterator<char>(in), {}};
    }

    std::string user_dir() const { return (root / "user").string(); }
};

} // namespace

TEST_F(AutostartItemsTest, a_program_is_of_the_system_or_of_the_user)
{
    write("system", "net.desktop");
    write("user", "mine.desktop");
    write("system", "both.desktop");
    write("user", "both.desktop");

    const auto all = items();

    ASSERT_EQ(all.size(), 3u);
    EXPECT_TRUE(item(all, "net.desktop").system);
    EXPECT_FALSE(item(all, "mine.desktop").system);
    // the copy of the user does not make a program of the system its own
    EXPECT_TRUE(item(all, "both.desktop").system);
    EXPECT_EQ(item(all, "both.desktop").app.path,
              (root / "user" / "autostart" / "both.desktop"));
}

TEST_F(AutostartItemsTest, a_program_the_user_turned_off_is_in_the_list)
{
    write("system", "a.desktop");
    write("user", "a.desktop", "Hidden=true\n");
    write("user", "b.desktop");

    const auto all = items();

    EXPECT_EQ(item(all, "a.desktop").state, AutostartItem::State::Off);
    EXPECT_EQ(item(all, "b.desktop").state, AutostartItem::State::On);
}

TEST_F(AutostartItemsTest, a_program_that_is_turned_off_in_the_system_stays_so)
{
    write("system", "a.desktop", "Hidden=true\n");

    EXPECT_EQ(item(items(), "a.desktop").state, AutostartItem::State::Skipped);
}

TEST_F(AutostartItemsTest, a_program_that_can_not_start_says_why)
{
    write("user", "orca.desktop", "AutostartCondition=GSettings x y z\n");
    write("user", "gnome.desktop", "OnlyShowIn=GNOME;\n");
    write("user", "gone.desktop", "TryExec=missing\n");
    write("user", "off.desktop", "X-GNOME-Autostart-enabled=false\n");

    const auto all = items();

    for (const auto& i : all) {
        EXPECT_EQ(i.state, AutostartItem::State::Skipped) << i.app.id;
        EXPECT_FALSE(i.reason.empty()) << i.app.id;
    }
    EXPECT_EQ(all.size(), 4u);
}

TEST_F(AutostartItemsTest, the_name_of_a_stub_is_the_one_of_the_system_file)
{
    write("system", "a.desktop", "", "Exec=x\n");
    std::ofstream(root / "user" / "autostart" / "a.desktop")
            << "[Desktop Entry]\nType=Application\nHidden=true\n";

    EXPECT_EQ(item(items(), "a.desktop").name, "a.desktop");
}

TEST_F(AutostartItemsTest, what_is_not_an_application_is_not_in_the_list)
{
    std::ofstream(root / "user" / "autostart" / "link.desktop")
            << "[Desktop Entry]\nType=Link\nName=L\nURL=x\n";
    write("system", "link.desktop");

    EXPECT_TRUE(items().empty());
}

TEST_F(AutostartItemsTest, a_program_of_the_system_is_turned_off_by_a_stub)
{
    write("system", "a.desktop");
    ASSERT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), false));

    EXPECT_TRUE(ids().empty());
    EXPECT_EQ(read("user", "a.desktop"),
              "[Desktop Entry]\nType=Application\nHidden=true\n");
    EXPECT_NE(read("system", "a.desktop").find("Exec=program"),
              std::string::npos);
}

TEST_F(AutostartItemsTest, turning_a_program_of_the_system_on_removes_the_stub)
{
    write("system", "a.desktop");
    ASSERT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), false));

    ASSERT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), true));

    EXPECT_EQ(ids(), std::vector<std::string>{"a.desktop"});
    EXPECT_FALSE(fs::exists(root / "user" / "autostart" / "a.desktop"));
}

TEST_F(AutostartItemsTest, the_file_of_the_user_keeps_what_it_has)
{
    write("user", "a.desktop", "Icon=x\n[Desktop Action n]\nName=N\n");

    ASSERT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), false));
    EXPECT_TRUE(ids().empty());
    EXPECT_EQ(read("user", "a.desktop"),
              "[Desktop Entry]\nHidden=true\nType=Application\nName=a.desktop\n"
              "Exec=program\nIcon=x\n[Desktop Action n]\nName=N\n");

    ASSERT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), true));
    EXPECT_EQ(ids(), std::vector<std::string>{"a.desktop"});
    EXPECT_EQ(read("user", "a.desktop").find("Hidden"), std::string::npos);
}

TEST_F(AutostartItemsTest, a_copy_with_a_command_stays_when_it_is_turned_on)
{
    write("system", "a.desktop");
    write("user", "a.desktop", "Hidden=true\n");

    ASSERT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), true));

    EXPECT_EQ(ids(), std::vector<std::string>{"a.desktop"});
    EXPECT_TRUE(fs::exists(root / "user" / "autostart" / "a.desktop"));
}

TEST_F(AutostartItemsTest, the_directory_of_the_user_is_made_when_it_is_missing)
{
    write("system", "a.desktop");
    fs::remove_all(root / "user");

    EXPECT_TRUE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), false));
    EXPECT_TRUE(ids().empty());
}

TEST_F(AutostartItemsTest, a_directory_that_can_not_be_written_is_a_failure)
{
    write("system", "a.desktop");
    fs::remove_all(root / "user");
    std::ofstream(root / "user") << "a file, not a directory\n";

    EXPECT_FALSE(tiny::xdg::set_autostart_enabled(
            user_dir(), item(items(), "a.desktop"), false));
}

TEST_F(AutostartItemsTest,
       a_turned_off_program_that_could_not_start_is_not_switchable)
{
    write("user", "a.desktop", "Hidden=true\nTryExec=missing\n");

    EXPECT_EQ(item(items(), "a.desktop").state, AutostartItem::State::Skipped);
}
