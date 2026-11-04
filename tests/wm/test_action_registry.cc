#include <gtest/gtest.h>

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "wm/action_registry.h"

using tiny::wm::ActionRegistry;

TEST(ActionRegistryTest, a_registered_action_runs_by_its_name)
{
    ActionRegistry actions;
    int calls = 0;
    actions.add("count", [&calls] { ++calls; });

    EXPECT_TRUE(actions.has("count"));
    EXPECT_TRUE(actions.run("count"));
    EXPECT_TRUE(actions.run("count"));

    EXPECT_EQ(calls, 2);
}

TEST(ActionRegistryTest, an_unknown_name_is_not_run)
{
    ActionRegistry actions;

    EXPECT_FALSE(actions.has("nothing"));
    EXPECT_FALSE(actions.run("nothing"));
    EXPECT_FALSE(actions.run(""));
    EXPECT_FALSE(actions.run("exec:")) << "no command, nothing to run";
}

TEST(ActionRegistryTest, adding_a_name_again_replaces_the_action)
{
    ActionRegistry actions;
    int first = 0;
    int second = 0;
    actions.add("a", [&first] { ++first; });
    actions.add("a", [&second] { ++second; });

    actions.run("a");

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 1);
}

TEST(ActionRegistryTest, the_names_are_sorted)
{
    ActionRegistry actions;
    actions.add("b", [] {});
    actions.add("a", [] {});
    actions.add("c", [] {});

    EXPECT_EQ(actions.names(), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(ActionRegistryTest, exec_starts_a_command_line)
{
    const auto marker = std::filesystem::temp_directory_path() /
                        ("tiny-action-" + std::to_string(getpid()));
    std::filesystem::remove(marker);
    ActionRegistry actions;

    EXPECT_TRUE(actions.run("exec:touch " + marker.string()));

    bool done = false;
    for (int i = 0; i < 300 && !done; ++i) {
        done = std::filesystem::exists(marker);
        if (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    std::filesystem::remove(marker);
    EXPECT_TRUE(done);
}

TEST(ActionRegistryTest, exec_of_a_missing_program_is_reported_not_fatal)
{
    ActionRegistry actions;

    EXPECT_TRUE(actions.run("exec:/nonexistent/tiny-program"));
}
