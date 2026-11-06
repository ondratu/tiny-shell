#include <gtest/gtest.h>

#include "wm/sync_gate.h"

using namespace std::chrono_literals;
using tiny::wm::SyncGate;

namespace {

const SyncGate::Clock::time_point start = SyncGate::Clock::now();

} // namespace

TEST(SyncGateTest, is_open_until_a_request_is_made)
{
    const SyncGate gate;

    EXPECT_TRUE(gate.is_open(0, start));
    EXPECT_FALSE(gate.is_waiting());
}

TEST(SyncGateTest, a_request_closes_it_until_the_client_has_drawn)
{
    SyncGate gate;

    EXPECT_EQ(gate.request(start), 1u);

    EXPECT_TRUE(gate.is_waiting());
    EXPECT_FALSE(gate.is_open(0, start + 1ms));
    EXPECT_TRUE(gate.is_open(1, start + 1ms));
    EXPECT_TRUE(gate.is_open(7, start + 1ms)); // later than asked
}

TEST(SyncGateTest, every_request_has_a_new_number)
{
    SyncGate gate;

    EXPECT_EQ(gate.request(start), 1u);
    EXPECT_EQ(gate.request(start), 2u);
    EXPECT_EQ(gate.get_sent(), 2u);
    EXPECT_FALSE(gate.is_open(1, start));
    EXPECT_TRUE(gate.is_open(2, start));
}

TEST(SyncGateTest, a_client_that_does_not_answer_is_not_waited_for_long)
{
    SyncGate gate;
    gate.request(start);

    EXPECT_FALSE(gate.is_open(0, start + SyncGate::timeout));
    EXPECT_TRUE(gate.is_open(0, start + SyncGate::timeout + 1ms));
}

TEST(SyncGateTest, reset_forgets_the_waiting_but_not_the_numbers)
{
    SyncGate gate;
    gate.request(start);

    gate.reset();

    EXPECT_TRUE(gate.is_open(0, start));
    EXPECT_EQ(gate.request(start), 2u); // the counter of the client is at 1
}
