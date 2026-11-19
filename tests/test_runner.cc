#include <gtest/gtest.h>

#include <poll.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "runner.h"

using namespace std::chrono_literals;
using tiny::FdWatch;
using tiny::Runner;
using tiny::RunResult;
using tiny::Timers;

namespace {

//! The loop of the shell, without X: runs until `done` or a second passes
class Loop {
  public:
    Timers timers;
    FdWatch watches;

    template <class Done>
    bool run_until(Done done)
    {
        const auto end = Timers::Clock::now() + 5s;
        while (!done() && Timers::Clock::now() < end) {
            std::vector<pollfd> fds;
            std::vector<FdWatch::Id> ids;
            watches.prepare(fds, ids);
            const int wait = std::min(timers.timeout_ms(), 50);
            poll(fds.data(), fds.size(), wait < 0 ? 50 : wait);
            timers.run_due();
            watches.dispatch(fds, ids, 0);
        }
        return done();
    }
};

} // namespace

TEST(RunnerTest, collects_the_output_and_the_status)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    std::optional<RunResult> got;
    ASSERT_TRUE(runner.run({"sh", "-c", "printf 'a b'; exit 0"},
                           [&](const RunResult& r) { got = r; }));
    EXPECT_EQ(runner.running(), 1u);
    ASSERT_TRUE(loop.run_until([&] { return got.has_value(); }));
    EXPECT_TRUE(got->ok);
    EXPECT_EQ(got->status, 0);
    EXPECT_EQ(got->output, "a b");
    EXPECT_FALSE(got->timed_out);
    EXPECT_EQ(runner.running(), 0u);
    EXPECT_TRUE(loop.watches.empty());
    EXPECT_TRUE(loop.timers.empty());
}

TEST(RunnerTest, a_failure_is_not_ok)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    std::optional<RunResult> got;
    ASSERT_TRUE(runner.run({"sh", "-c", "echo oops; exit 3"},
                           [&](const RunResult& r) { got = r; }));
    ASSERT_TRUE(loop.run_until([&] { return got.has_value(); }));
    EXPECT_FALSE(got->ok);
    EXPECT_EQ(got->status, 3);
    EXPECT_EQ(got->output, "oops\n");
}

TEST(RunnerTest, a_signal_is_a_status_of_minus_one)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    std::optional<RunResult> got;
    ASSERT_TRUE(runner.run({"sh", "-c", "kill -9 $$"},
                           [&](const RunResult& r) { got = r; }));
    ASSERT_TRUE(loop.run_until([&] { return got.has_value(); }));
    EXPECT_FALSE(got->ok);
    EXPECT_EQ(got->status, -1);
}

TEST(RunnerTest, a_program_that_does_not_exist_is_not_started)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    bool called = false;
    errno = 0;
    EXPECT_FALSE(runner.run({"/nonexistent/tiny-program"},
                            [&](const RunResult&) { called = true; }));
    EXPECT_EQ(errno, ENOENT);
    EXPECT_FALSE(runner.run({}, [&](const RunResult&) { called = true; }));
    EXPECT_EQ(runner.running(), 0u);
    EXPECT_FALSE(called);
}

TEST(RunnerTest, a_program_that_is_too_slow_is_killed)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    std::optional<RunResult> got;
    Runner::Options options;
    options.timeout = 100ms;
    ASSERT_TRUE(runner.run(
            {"sh", "-c", "echo start; sleep 30"},
            [&](const RunResult& r) { got = r; }, options));
    ASSERT_TRUE(loop.run_until([&] { return got.has_value(); }));
    EXPECT_TRUE(got->timed_out);
    EXPECT_FALSE(got->ok);
    EXPECT_EQ(got->output, "start\n");
    EXPECT_TRUE(loop.watches.empty());
}

TEST(RunnerTest, the_output_is_cut_at_the_limit)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    std::optional<RunResult> got;
    Runner::Options options;
    options.max_output = 10;
    ASSERT_TRUE(runner.run(
            {"sh", "-c", "printf '0123456789abcdef'"},
            [&](const RunResult& r) { got = r; }, options));
    ASSERT_TRUE(loop.run_until([&] { return got.has_value(); }));
    EXPECT_EQ(got->output, "0123456789");
    EXPECT_TRUE(got->truncated);
    EXPECT_TRUE(got->ok); // it ended well, only the output is not all here
}

TEST(RunnerTest, a_function_may_start_another_program)
{
    Loop loop;
    Runner runner(loop.timers, loop.watches);
    std::string seen;
    ASSERT_TRUE(runner.run({"echo", "one"}, [&](const RunResult& first) {
        seen = first.output;
        EXPECT_TRUE(runner.run({"echo", "two"}, [&](const RunResult& second) {
            seen += second.output;
        }));
    }));
    ASSERT_TRUE(loop.run_until([&] { return seen == "one\ntwo\n"; }));
}

TEST(RunnerTest, cancel_kills_the_programs_and_calls_nothing)
{
    Loop loop;
    bool called = false;
    {
        Runner runner(loop.timers, loop.watches);
        ASSERT_TRUE(runner.run({"sleep", "30"},
                               [&](const RunResult&) { called = true; }));
        runner.cancel_all();
        EXPECT_EQ(runner.running(), 0u);
        ASSERT_TRUE(runner.run({"sleep", "30"},
                               [&](const RunResult&) { called = true; }));
    } // the destructor kills the second one
    EXPECT_FALSE(called);
    EXPECT_TRUE(loop.watches.empty());
    EXPECT_TRUE(loop.timers.empty());
}
