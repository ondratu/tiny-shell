#include <gtest/gtest.h>

#include <vector>

#include "timers.h"

using namespace std::chrono_literals;
using tiny::Timers;

namespace {
const Timers::Clock::time_point t0 = Timers::Clock::time_point{} + 1000s;
}

TEST(TimersTest, no_timer_means_sleep_forever)
{
    Timers timers;
    EXPECT_TRUE(timers.empty());
    EXPECT_EQ(timers.timeout_ms(t0), -1);
    EXPECT_EQ(timers.run_due(t0), 0);
}

TEST(TimersTest, timeout_is_the_time_to_the_earliest_timer)
{
    Timers timers;
    timers.after(500ms, [] {}, t0);
    timers.after(200ms, [] {}, t0);
    EXPECT_EQ(timers.timeout_ms(t0), 200);
    EXPECT_EQ(timers.timeout_ms(t0 + 50ms), 150);
    EXPECT_EQ(timers.timeout_ms(t0 + 5s), 0); // late
}

TEST(TimersTest, timeout_is_rounded_up)
{
    Timers timers;
    timers.after(10ms, [] {}, t0);
    EXPECT_EQ(timers.timeout_ms(t0 + 9500us), 1);
}

TEST(TimersTest, a_timer_fires_once_when_it_is_due)
{
    Timers timers;
    int count = 0;
    timers.after(100ms, [&] { ++count; }, t0);
    EXPECT_EQ(timers.run_due(t0 + 99ms), 0);
    EXPECT_EQ(timers.run_due(t0 + 100ms), 1);
    EXPECT_EQ(timers.run_due(t0 + 200ms), 0);
    EXPECT_EQ(count, 1);
    EXPECT_TRUE(timers.empty());
}

TEST(TimersTest, due_timers_run_in_the_order_of_time)
{
    Timers timers;
    std::vector<int> order;
    timers.after(300ms, [&] { order.push_back(3); }, t0);
    timers.after(100ms, [&] { order.push_back(1); }, t0);
    timers.after(200ms, [&] { order.push_back(2); }, t0);
    EXPECT_EQ(timers.run_due(t0 + 1s), 3);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(TimersTest, a_negative_delay_is_due_at_once)
{
    Timers timers;
    int count = 0;
    timers.after(-5ms, [&] { ++count; }, t0);
    EXPECT_EQ(timers.run_due(t0), 1);
}

TEST(TimersTest, a_periodic_timer_repeats_without_catching_up)
{
    Timers timers;
    int count = 0;
    timers.every(100ms, [&] { ++count; }, t0);
    EXPECT_EQ(timers.run_due(t0 + 100ms), 1);
    EXPECT_EQ(timers.timeout_ms(t0 + 100ms), 100);
    // the loop was blocked for a long time: one call, not ten
    EXPECT_EQ(timers.run_due(t0 + 1100ms), 1);
    EXPECT_EQ(timers.run_due(t0 + 1100ms), 0);
    EXPECT_EQ(count, 2);
    EXPECT_FALSE(timers.empty());
}

TEST(TimersTest, a_zero_period_does_not_spin)
{
    Timers timers;
    timers.every(0ms, [] {}, t0);
    EXPECT_EQ(timers.run_due(t0), 0);
    EXPECT_EQ(timers.timeout_ms(t0), 1);
}

TEST(TimersTest, a_cancelled_timer_does_not_run)
{
    Timers timers;
    int count = 0;
    const auto id = timers.after(10ms, [&] { ++count; }, t0);
    EXPECT_TRUE(timers.cancel(id));
    EXPECT_FALSE(timers.cancel(id));
    EXPECT_EQ(timers.run_due(t0 + 1s), 0);
    EXPECT_EQ(count, 0);
}

TEST(TimersTest, a_timer_can_cancel_another_one_that_is_due)
{
    Timers timers;
    int count = 0;
    Timers::Id second = 0;
    timers.after(10ms, [&] { timers.cancel(second); }, t0);
    second = timers.after(20ms, [&] { ++count; }, t0);
    EXPECT_EQ(timers.run_due(t0 + 1s), 1);
    EXPECT_EQ(count, 0);
}

TEST(TimersTest, a_timer_can_cancel_itself_and_add_a_new_one)
{
    Timers timers;
    int count = 0;
    Timers::Id id = 0;
    id = timers.every(
            10ms,
            [&] {
                timers.cancel(id);
                timers.after(0ms, [&] { ++count; }, t0 + 1s);
            },
            t0);
    EXPECT_EQ(timers.run_due(t0 + 1s), 1);
    EXPECT_EQ(count, 0); // the new one waits for the next round
    EXPECT_EQ(timers.run_due(t0 + 1s), 1);
    EXPECT_EQ(count, 1);
}

TEST(TimersTest, a_periodic_timer_keeps_its_function_between_rounds)
{
    Timers timers;
    int count = 0;
    timers.every(10ms, [&] { ++count; }, t0);
    for (int i = 1; i <= 3; ++i) {
        EXPECT_EQ(timers.run_due(t0 + i * 10ms), 1);
    }
    EXPECT_EQ(count, 3);
}
