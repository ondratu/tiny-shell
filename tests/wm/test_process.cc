#include <gtest/gtest.h>

#include <unistd.h>

#include "wm/process.h"

using tiny::wm::killable_pid;

TEST(ProcessTest, the_host_name_is_not_empty)
{
    EXPECT_FALSE(tiny::wm::local_host_name().empty());
}

TEST(ProcessTest, a_process_of_this_machine_can_be_killed)
{
    EXPECT_EQ(killable_pid(1234, "box", "box"), 1234);
}

TEST(ProcessTest, a_process_of_another_machine_can_not)
{
    EXPECT_EQ(killable_pid(1234, "other", "box"), 0);
}

TEST(ProcessTest, a_client_that_does_not_tell_its_machine_can_not)
{
    EXPECT_EQ(killable_pid(1234, "", "box"), 0);
    EXPECT_EQ(killable_pid(1234, "", ""), 0);
}

TEST(ProcessTest, no_pid_means_no_process)
{
    EXPECT_EQ(killable_pid(0, "box", "box"), 0);
}

TEST(ProcessTest, the_init_is_never_killed)
{
    EXPECT_EQ(killable_pid(1, "box", "box"), 0);
}

TEST(ProcessTest, the_window_manager_is_never_killed)
{
    EXPECT_EQ(killable_pid(static_cast<unsigned long>(getpid()), "box", "box"),
              0);
}

TEST(ProcessTest, a_pid_out_of_range_is_ignored)
{
    EXPECT_EQ(killable_pid(0xFFFFFFFFUL, "box", "box"), 0);
}
