#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <clocale>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "xdg/application_index.h"

namespace fs = std::filesystem;

namespace {

class ApplicationIndexTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-apps" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "user" / "applications");
        fs::create_directories(root / "system" / "applications");
        fs::create_directories(root / "bin");
    }

    void TearDown() override { fs::remove_all(root); }

    void write(const char* where, const std::string& name,
               const std::string& body)
    {
        const auto file = root / where / "applications" / name;
        fs::create_directories(file.parent_path());
        std::ofstream(file) << body;
    }

    static std::string app(const std::string& name,
                           const std::string& extra = "")
    {
        return "[Desktop Entry]\nType=Application\nName=" + name +
               "\nExec=" + name + "\n" + extra;
    }

    std::vector<std::string> dirs() const
    {
        return {(root / "user" / "applications").string(),
                (root / "system" / "applications").string()};
    }

    fs::path root;
};

} // namespace

TEST(ApplicationDirsTest, defaults)
{
    const auto dirs = tiny::xdg::application_dirs(nullptr, nullptr, "/home/a");
    ASSERT_EQ(dirs.size(), 3u);
    EXPECT_EQ(dirs[0], "/home/a/.local/share/applications");
    EXPECT_EQ(dirs[1], "/usr/local/share/applications");
    EXPECT_EQ(dirs[2], "/usr/share/applications");
}

TEST(ApplicationDirsTest, from_variables)
{
    const auto dirs = tiny::xdg::application_dirs("/h", "/a::/b", "/home/a");
    ASSERT_EQ(dirs.size(), 3u);
    EXPECT_EQ(dirs[0], "/h/applications");
    EXPECT_EQ(dirs[1], "/a/applications");
    EXPECT_EQ(dirs[2], "/b/applications");
}

TEST(ApplicationDirsTest, empty_values_are_unset)
{
    EXPECT_EQ(tiny::xdg::application_dirs("", "", "").size(), 2u);
    EXPECT_EQ(tiny::xdg::application_dirs(nullptr, nullptr, nullptr).size(),
              2u);
}

TEST(ApplicationDirsTest, relative_paths_are_ignored)
{
    const auto dirs = tiny::xdg::application_dirs("share", "rel:/b", "/home/a");
    ASSERT_EQ(dirs.size(), 2u);
    EXPECT_EQ(dirs[0], "/home/a/.local/share/applications");
    EXPECT_EQ(dirs[1], "/b/applications");
    EXPECT_TRUE(tiny::xdg::application_dirs("share", "rel", "home").empty());
}

TEST(ApplicationDirsTest, trailing_slashes_and_duplicates_are_dropped)
{
    const auto dirs =
            tiny::xdg::application_dirs("/h/", "/a/://a:/b//:/", "/x");
    ASSERT_EQ(dirs.size(), 4u);
    EXPECT_EQ(dirs[0], "/h/applications");
    EXPECT_EQ(dirs[1], "/a/applications");
    EXPECT_EQ(dirs[2], "/b/applications");
    EXPECT_EQ(dirs[3], "/applications");
}

TEST_F(ApplicationIndexTest, executable_exists)
{
    const auto script = root / "bin" / "tool";
    std::ofstream(script) << "#!/bin/sh\n";
    chmod(script.c_str(), 0755);
    std::ofstream(root / "bin" / "plain") << "x";
    const std::string path = "/nonexistent:" + (root / "bin").string();

    EXPECT_TRUE(tiny::xdg::executable_exists("tool", path));
    EXPECT_TRUE(tiny::xdg::executable_exists(script.string(), ""));
    EXPECT_FALSE(tiny::xdg::executable_exists("plain", path));
    EXPECT_FALSE(tiny::xdg::executable_exists("tool", ""));
    EXPECT_FALSE(
            tiny::xdg::executable_exists("bin/tool", path)); // not a lookup
    EXPECT_FALSE(tiny::xdg::executable_exists("", path));
    EXPECT_FALSE(tiny::xdg::executable_exists((root / "bin").string(), ""));
}

TEST_F(ApplicationIndexTest, sorted_by_name_ignoring_case)
{
    write("user", "b.desktop", app("beta"));
    write("system", "a.desktop", app("Alpha"));
    write("system", "c.desktop", app("gamma"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    ASSERT_EQ(index.applications().size(), 3u);
    EXPECT_EQ(index.applications()[0].entry.name, "Alpha");
    EXPECT_EQ(index.applications()[1].entry.name, "beta");
    EXPECT_EQ(index.applications()[2].entry.name, "gamma");
    EXPECT_EQ(index.applications()[0].id, "a.desktop");
}

TEST_F(ApplicationIndexTest, earlier_directory_wins)
{
    write("user", "x.desktop", app("User"));
    write("system", "x.desktop", app("System"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    ASSERT_EQ(index.applications().size(), 1u);
    EXPECT_EQ(index.applications()[0].entry.name, "User");
}

TEST_F(ApplicationIndexTest, hidden_file_masks_the_later_one)
{
    write("user", "x.desktop", app("User", "Hidden=true\n"));
    write("system", "x.desktop", app("System"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    EXPECT_TRUE(index.applications().empty());
}

TEST_F(ApplicationIndexTest, broken_file_hides_nothing)
{
    write("user", "x.desktop", app("User"));
    chmod((root / "user" / "applications" / "x.desktop").c_str(), 0);
    fs::create_directory(root / "user" / "applications" / "y.desktop");
    write("system", "x.desktop", app("System"));
    write("system", "y.desktop", app("Other"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    // root can read the unreadable file, so only y is checked for sure
    const auto found = std::any_of(index.applications().begin(),
                                   index.applications().end(),
                                   [](const tiny::xdg::Application& a) {
                                       return a.entry.name == "Other";
                                   });
    EXPECT_TRUE(found);
}

TEST_F(ApplicationIndexTest, unreadable_directory_does_not_stop_the_scan)
{
    write("system", "a/inside.desktop", app("Inside"));
    write("system", "z.desktop", app("Zed"));
    chmod((root / "system" / "applications" / "a").c_str(), 0);
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());
    chmod((root / "system" / "applications" / "a").c_str(), 0755);

    const auto& apps = index.applications();
    EXPECT_TRUE(std::any_of(apps.begin(), apps.end(),
                            [](const tiny::xdg::Application& a) {
                                return a.entry.name == "Zed";
                            }));
}

TEST_F(ApplicationIndexTest, loop_of_links_ends)
{
    write("system", "a.desktop", app("Alpha"));
    fs::create_directory_symlink(root / "system" / "applications",
                                 root / "system" / "applications" / "loop");
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    ASSERT_EQ(index.applications().size(), 1u);
    EXPECT_EQ(index.applications()[0].id, "a.desktop");
}

TEST_F(ApplicationIndexTest, equal_names_are_sorted_by_id)
{
    write("system", "b.desktop", app("Same"));
    write("system", "a.desktop", app("Same"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    ASSERT_EQ(index.applications().size(), 2u);
    EXPECT_EQ(index.applications()[0].id, "a.desktop");
    EXPECT_EQ(index.applications()[1].id, "b.desktop");
}

TEST_F(ApplicationIndexTest, names_are_sorted_by_the_collation_of_the_locale)
{
    const std::string saved = std::setlocale(LC_COLLATE, nullptr);
    if (std::setlocale(LC_COLLATE, "cs_CZ.UTF-8") == nullptr) {
        GTEST_SKIP() << "cs_CZ.UTF-8 is not installed";
    }
    // musl accepts any locale name, but its strcoll compares bytes
    if (std::strcoll("Čtečka", "Zebra") >= 0) {
        std::setlocale(LC_COLLATE, saved.c_str());
        GTEST_SKIP() << "the C library does not collate by locale";
    }
    write("system", "a.desktop", app("Zebra"));
    write("system", "b.desktop", app("Čtečka"));
    write("system", "c.desktop", app("Chata"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());
    std::setlocale(LC_COLLATE, saved.c_str());

    ASSERT_EQ(index.applications().size(), 3u);
    EXPECT_EQ(index.applications()[0].entry.name, "Čtečka");
    EXPECT_EQ(index.applications()[1].entry.name, "Chata"); // "ch" is a letter
    EXPECT_EQ(index.applications()[2].entry.name, "Zebra");
}

TEST_F(ApplicationIndexTest, ids_from_subdirectories_may_collide)
{
    write("system", "a-b/x.desktop", app("Deep"));
    write("system", "a/b-x.desktop", app("Other"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    ASSERT_EQ(index.applications().size(), 1u); // the first by path wins
    EXPECT_EQ(index.applications()[0].entry.name, "Other");
}

TEST_F(ApplicationIndexTest, try_exec_with_a_slash_is_a_path)
{
    const auto tool = root / "bin" / "tool";
    std::ofstream(tool) << "#!/bin/sh\n";
    chmod(tool.c_str(), 0755);
    write("system", "abs.desktop",
          app("Abs", "TryExec=" + tool.string() + "\n"));
    write("system", "bad.desktop", app("Bad", "TryExec=/nonexistent/x\n"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs(), "", "");

    ASSERT_EQ(index.applications().size(), 1u);
    EXPECT_EQ(index.applications()[0].entry.name, "Abs");
}

TEST_F(ApplicationIndexTest, try_exec_is_looked_up_in_the_environment_path)
{
    const auto tool = root / "bin" / "tool";
    std::ofstream(tool) << "#!/bin/sh\n";
    chmod(tool.c_str(), 0755);
    write("system", "ok.desktop", app("Ok", "TryExec=tool\n"));
    const char* saved = std::getenv("PATH");
    const std::string old = saved != nullptr ? saved : "";
    setenv("PATH", (root / "bin").c_str(), 1);
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());
    setenv("PATH", old.c_str(), 1);

    EXPECT_EQ(index.applications().size(), 1u);
}

TEST_F(ApplicationIndexTest, subdirectory_becomes_a_prefix_of_the_id)
{
    write("system", "kde/okular.desktop", app("Okular"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    ASSERT_EQ(index.applications().size(), 1u);
    EXPECT_EQ(index.applications()[0].id, "kde-okular.desktop");
}

TEST_F(ApplicationIndexTest, skips_invisible_and_other_files)
{
    write("system", "nodisplay.desktop", app("A", "NoDisplay=true\n"));
    write("system", "link.desktop",
          "[Desktop Entry]\nType=Link\nName=L\nURL=x\n");
    write("system", "notes.txt", app("B"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs());

    EXPECT_TRUE(index.applications().empty());
}

TEST_F(ApplicationIndexTest, try_exec_drops_missing_binaries)
{
    const auto tool = root / "bin" / "tool";
    std::ofstream(tool) << "#!/bin/sh\n";
    chmod(tool.c_str(), 0755);
    write("system", "ok.desktop", app("Ok", "TryExec=tool\n"));
    write("system", "missing.desktop", app("Missing", "TryExec=nothing\n"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs(), "", (root / "bin").string());

    ASSERT_EQ(index.applications().size(), 1u);
    EXPECT_EQ(index.applications()[0].entry.name, "Ok");
}

TEST_F(ApplicationIndexTest, locale_is_used_and_scan_replaces_the_content)
{
    write("system", "a.desktop", app("Tool", "Name[cs]=Nástroj\n"));
    tiny::xdg::ApplicationIndex index;
    index.scan(dirs(), "cs_CZ.UTF-8");
    ASSERT_EQ(index.applications().size(), 1u);
    EXPECT_EQ(index.applications()[0].entry.name, "Nástroj");

    index.scan({});
    EXPECT_TRUE(index.applications().empty());
}

TEST_F(ApplicationIndexTest, missing_directory_is_ignored)
{
    tiny::xdg::ApplicationIndex index;
    index.scan({(root / "none").string()});
    EXPECT_TRUE(index.applications().empty());
}
