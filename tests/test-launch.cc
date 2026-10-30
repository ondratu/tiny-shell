#include <gtest/gtest.h>

#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "launch.h"

namespace {

using Args = std::vector<std::string>;

tiny::Application app(const std::string& exec)
{
    tiny::Application application;
    application.id = "a.desktop";
    application.path = "/usr/share/applications/a.desktop";
    application.entry.name = "Alpha";
    application.entry.icon = "alpha";
    application.entry.exec = exec;
    return application;
}

template <class Done>
bool wait_for(Done done)
{
    for (int i = 0; i < 300; ++i) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::filesystem::path temp_path(const char* name)
{
    auto path = std::filesystem::temp_directory_path() /
                (std::string(name) + std::to_string(getpid()));
    std::filesystem::remove_all(path);
    return path;
}

} // namespace

TEST(ExpandExec, plain_command_is_split)
{
    EXPECT_EQ(tiny::expand_exec(app("prog -a \"b c\""), {}),
              (Args{"prog", "-a", "b c"}));
    EXPECT_TRUE(tiny::expand_exec(app(""), {}).empty());
}

TEST(ExpandExec, single_file_codes_take_the_first_file)
{
    EXPECT_EQ(tiny::expand_exec(app("prog %f"), {"/a", "/b"}),
              (Args{"prog", "/a"}));
    EXPECT_EQ(tiny::expand_exec(app("prog --open=%u"), {"http://x"}),
              (Args{"prog", "--open=http://x"}));
}

TEST(ExpandExec, list_codes_take_all_files)
{
    EXPECT_EQ(tiny::expand_exec(app("prog %F"), {"/a", "/b"}),
              (Args{"prog", "/a", "/b"}));
    EXPECT_EQ(tiny::expand_exec(app("prog %U end"), {"u"}),
              (Args{"prog", "u", "end"}));
}

TEST(ExpandExec, file_codes_without_a_file_vanish)
{
    EXPECT_EQ(tiny::expand_exec(app("prog %f %F %u %U"), {}), (Args{"prog"}));
    EXPECT_EQ(tiny::expand_exec(app("prog x%f"), {}), (Args{"prog", "x"}));
}

TEST(ExpandExec, entry_codes)
{
    EXPECT_EQ(tiny::expand_exec(app("prog %i %c %k"), {}),
              (Args{"prog", "--icon", "alpha", "Alpha",
                    "/usr/share/applications/a.desktop"}));

    auto no_icon = app("prog %i end");
    no_icon.entry.icon.clear();
    EXPECT_EQ(tiny::expand_exec(no_icon, {}), (Args{"prog", "end"}));
}

TEST(ExpandExec, percent_and_unknown_codes)
{
    EXPECT_EQ(tiny::expand_exec(app("prog 100%% %d %n %v -%m-"), {}),
              (Args{"prog", "100%", "--"}));
    EXPECT_EQ(tiny::expand_exec(app("prog 50%"), {}), (Args{"prog", "50%"}));
}

TEST(ExpandExec, list_and_icon_codes_inside_an_argument_drop_it)
{
    EXPECT_EQ(tiny::expand_exec(app("prog --files=%F --icon=%i -u%U end"),
                                {"/a", "/b"}),
              (Args{"prog", "end"}));
}

TEST(ExpandExec, a_file_is_not_expanded_again)
{
    EXPECT_EQ(tiny::expand_exec(app("prog %f %c"), {"%c"}),
              (Args{"prog", "%c", "Alpha"}));
}

TEST(Launch, runs_in_the_path_directory)
{
    const auto dir = temp_path("tiny-launch-dir-");
    std::filesystem::create_directories(dir);

    auto application = app("touch marker");
    application.entry.path = dir.string();
    EXPECT_GT(tiny::launch(application), 0);
    EXPECT_TRUE(
            wait_for([&] { return std::filesystem::exists(dir / "marker"); }));
    std::filesystem::remove_all(dir);
}

TEST(Launch, missing_path_directory_is_reported)
{
    auto application = app("true");
    application.entry.path = "/nonexistent/dir";
    errno = 0;
    EXPECT_EQ(tiny::launch(application), -1);
    EXPECT_EQ(errno, ENOENT);
}

TEST(Launch, terminal_application_runs_in_the_terminal)
{
    const auto marker = temp_path("tiny-launch-term-");
    auto application = app("a b");
    application.entry.terminal = true;
    // the "terminal" writes its arguments to the marker
    EXPECT_GT(tiny::launch(
                      application, {},
                      {"sh", "-c", "echo \"$@\" > " + marker.string(), "sh"}),
              0);
    EXPECT_TRUE(wait_for([&] {
        return std::filesystem::exists(marker) &&
               std::filesystem::file_size(marker) > 0;
    }));
    std::ifstream in(marker);
    std::string line;
    std::getline(in, line);
    EXPECT_EQ(line, "a b");
    std::filesystem::remove(marker);
}

TEST(Launch, entry_without_a_command_is_refused)
{
    errno = 0;
    EXPECT_EQ(tiny::launch(app("")), -1);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(tiny::launch(app("%f")), -1);
}

TEST(Launch, unknown_program_is_reported)
{
    errno = 0;
    EXPECT_EQ(tiny::launch(app("/nonexistent/program %f")), -1);
    EXPECT_EQ(errno, ENOENT);
}
