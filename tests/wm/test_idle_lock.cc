#include <gtest/gtest.h>

#include <optional>

#include "wm/idle_lock.h"

using std::chrono::milliseconds;
using std::chrono::minutes;
using std::chrono::seconds;
using tiny::wm::IdleLock;

namespace {

//! The hooks are variables: what the idle time is, whether it is locked
struct FakeUser {
    IdleLock make(minutes limit)
    {
        IdleLock lock({[this] { return idle; }, [this] { return locked; },
                       [this] {
                           ++locks;
                           locked = true;
                       }});
        lock.set_limit(limit);
        return lock;
    }

    std::optional<milliseconds> idle = milliseconds(0);
    bool locked = false;
    int locks = 0;
};

} // namespace

TEST(IdleLockTest, nothing_is_done_without_a_limit)
{
    FakeUser user;
    user.idle = minutes(100);
    const IdleLock lock = user.make(minutes(0));

    EXPECT_FALSE(lock.check());
    EXPECT_EQ(user.locks, 0);
}

TEST(IdleLockTest, it_waits_for_what_is_left_of_the_limit)
{
    FakeUser user;
    user.idle = minutes(3);
    const IdleLock lock = user.make(minutes(10));

    EXPECT_EQ(lock.check(), minutes(7));
    EXPECT_EQ(user.locks, 0);
}

TEST(IdleLockTest, the_screen_is_locked_when_the_limit_is_reached)
{
    FakeUser user;
    user.idle = minutes(10);
    const IdleLock lock = user.make(minutes(10));

    EXPECT_EQ(lock.check(), minutes(10));
    EXPECT_EQ(user.locks, 1);
}

TEST(IdleLockTest, a_locked_screen_is_not_locked_again)
{
    FakeUser user;
    user.idle = minutes(30);
    user.locked = true;
    const IdleLock lock = user.make(minutes(10));

    EXPECT_EQ(lock.check(), minutes(10));
    EXPECT_EQ(user.locks, 0);
}

TEST(IdleLockTest, a_short_rest_is_not_shorter_than_a_second)
{
    FakeUser user;
    user.idle = minutes(10) - milliseconds(5);
    const IdleLock lock = user.make(minutes(10));

    EXPECT_EQ(lock.check(), seconds(1));
    EXPECT_EQ(user.locks, 0);
}

TEST(IdleLockTest, an_unknown_idle_time_stops_the_checks)
{
    FakeUser user;
    user.idle = std::nullopt;
    const IdleLock lock = user.make(minutes(10));

    EXPECT_FALSE(lock.check());
    EXPECT_EQ(user.locks, 0);
}

TEST(IdleLockTest, the_user_who_came_back_is_waited_for_again)
{
    FakeUser user;
    const IdleLock lock = user.make(minutes(5));

    user.idle = minutes(5);
    lock.check();
    user.locked = false; // unlocked, the input has reset the idle time
    user.idle = seconds(30);

    EXPECT_EQ(lock.check(), minutes(5) - seconds(30));
    EXPECT_EQ(user.locks, 1);
}
