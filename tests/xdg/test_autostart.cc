#include "helpers.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
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
