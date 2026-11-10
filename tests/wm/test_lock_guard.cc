#include <gtest/gtest.h>
#include <sys/wait.h>

#include <vector>

#include "wm/lock_guard.h"

using tiny::wm::LockGuard;

namespace {

//! A lock that is a script: `ended` says what is set, `starts` counts
struct FakeLock {
    LockGuard make()
    {
        return LockGuard(
                {[this] {
                     ++starts;
                     return can_start ? next_pid++ : -1;
                 },
                 [this](pid_t, int* status) {
                     *status = wait_status;
                     return has_ended;
                 },
                 [this](const std::string& text) { reports.push_back(text); }});
    }

    void end_with_exit(int code)
    {
        has_ended = true;
        wait_status = code << 8; // what WIFEXITED / WEXITSTATUS read
    }

    void end_with_signal(int signal_number)
    {
        has_ended = true;
        wait_status = signal_number;
    }

    int starts = 0;
    pid_t next_pid = 100;
    bool can_start = true;
    bool has_ended = false;
    int wait_status = 0;
    std::vector<std::string> reports;
};

} // namespace

TEST(LockGuardTest, it_starts_the_lock)
{
    FakeLock fake;
    LockGuard guard = fake.make();

    guard.lock();

    EXPECT_EQ(fake.starts, 1);
    EXPECT_TRUE(guard.running());
}

TEST(LockGuardTest, a_lock_that_runs_is_not_started_again)
{
    FakeLock fake;
    LockGuard guard = fake.make();

    guard.lock();
    guard.lock();

    EXPECT_EQ(fake.starts, 1);
}

TEST(LockGuardTest, a_lock_that_can_not_be_started_is_reported)
{
    FakeLock fake;
    fake.can_start = false;
    LockGuard guard = fake.make();

    guard.lock();

    EXPECT_FALSE(guard.running());
    ASSERT_EQ(fake.reports.size(), 1u);
    EXPECT_NE(fake.reports[0].find("not locked"), std::string::npos);
}

TEST(LockGuardTest, nothing_happens_while_the_lock_runs)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();

    guard.check();

    EXPECT_TRUE(guard.running());
    EXPECT_EQ(fake.starts, 1);
    EXPECT_TRUE(fake.reports.empty());
}

TEST(LockGuardTest, an_unlock_ends_the_watching)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_exit(0);

    guard.check();

    EXPECT_FALSE(guard.running());
    EXPECT_EQ(fake.starts, 1);
    EXPECT_TRUE(fake.reports.empty());
}

TEST(LockGuardTest, a_lock_that_could_not_lock_is_not_started_again)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_exit(LockGuard::cant_lock_status);

    guard.check();

    EXPECT_FALSE(guard.running());
    EXPECT_EQ(fake.starts, 1);
    ASSERT_EQ(fake.reports.size(), 1u);
    EXPECT_NE(fake.reports[0].find("not locked"), std::string::npos);
}

TEST(LockGuardTest, a_crash_starts_the_lock_again)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_signal(SIGSEGV);

    guard.check();

    EXPECT_TRUE(guard.running());
    EXPECT_EQ(fake.starts, 2);
    ASSERT_EQ(fake.reports.size(), 1u);
}

TEST(LockGuardTest, another_exit_status_is_a_crash_too)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_exit(2);

    guard.check();

    EXPECT_EQ(fake.starts, 2);
}

TEST(LockGuardTest, a_kill_is_a_crash_too)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_signal(SIGKILL);

    guard.check();

    EXPECT_EQ(fake.starts, 2);
}

TEST(LockGuardTest, a_lock_that_keeps_dying_is_given_up)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_signal(SIGSEGV);

    for (int i = 0; i < LockGuard::max_restarts; ++i) {
        guard.check();
        ASSERT_TRUE(guard.running()) << i;
    }
    guard.check(); // one more than it may

    EXPECT_FALSE(guard.running());
    EXPECT_EQ(fake.starts, 1 + LockGuard::max_restarts);
    EXPECT_NE(fake.reports.back().find("not started again"), std::string::npos);
}

TEST(LockGuardTest, a_new_lock_starts_the_count_again)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.end_with_signal(SIGSEGV);
    guard.check();
    fake.end_with_exit(0);
    guard.check();
    ASSERT_FALSE(guard.running());

    fake.has_ended = false;
    guard.lock();
    fake.end_with_signal(SIGSEGV);
    for (int i = 0; i < LockGuard::max_restarts; ++i) {
        guard.check();
        EXPECT_TRUE(guard.running()) << i;
    }
}

TEST(LockGuardTest, a_restart_that_fails_is_reported_and_ends)
{
    FakeLock fake;
    LockGuard guard = fake.make();
    guard.lock();
    fake.can_start = false;
    fake.end_with_signal(SIGSEGV);

    guard.check();

    EXPECT_FALSE(guard.running());
    EXPECT_GE(fake.reports.size(), 2u);
}

TEST(LockGuardTest, check_without_a_lock_does_nothing)
{
    FakeLock fake;
    LockGuard guard = fake.make();

    guard.check();

    EXPECT_EQ(fake.starts, 0);
}
