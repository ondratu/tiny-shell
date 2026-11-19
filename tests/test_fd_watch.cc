#include <gtest/gtest.h>

#include <poll.h>
#include <unistd.h>

#include "fd_watch.h"

using tiny::FdWatch;

namespace {

struct Pipe {
    int fds[2] = {-1, -1};
    Pipe() { EXPECT_EQ(pipe(fds), 0); }
    ~Pipe()
    {
        close(fds[0]);
        close(fds[1]);
    }
    void poke() const { EXPECT_EQ(write(fds[1], "x", 1), 1); }
};

//! One round of the loop: poll and dispatch
int round(FdWatch& watches)
{
    std::vector<pollfd> fds;
    std::vector<FdWatch::Id> ids;
    watches.prepare(fds, ids);
    poll(fds.data(), static_cast<nfds_t>(fds.size()), 100);
    return watches.dispatch(fds, ids, 0);
}

} // namespace

TEST(FdWatchTest, calls_the_function_of_a_ready_descriptor)
{
    FdWatch watches;
    const Pipe quiet;
    const Pipe loud;
    int quiet_calls = 0;
    int loud_calls = 0;
    watches.add(quiet.fds[0], [&] { ++quiet_calls; });
    watches.add(loud.fds[0], [&] { ++loud_calls; });
    loud.poke();
    EXPECT_EQ(round(watches), 1);
    EXPECT_EQ(quiet_calls, 0);
    EXPECT_EQ(loud_calls, 1);
}

TEST(FdWatchTest, an_empty_set_has_nothing_to_call)
{
    FdWatch watches;
    EXPECT_TRUE(watches.empty());
    std::vector<pollfd> fds;
    std::vector<FdWatch::Id> ids;
    watches.prepare(fds, ids);
    EXPECT_TRUE(fds.empty());
    EXPECT_EQ(watches.dispatch(fds, ids, 0), 0);
}

TEST(FdWatchTest, a_removed_watch_is_not_called)
{
    FdWatch watches;
    const Pipe pipe;
    int calls = 0;
    const FdWatch::Id id = watches.add(pipe.fds[0], [&] { ++calls; });
    EXPECT_TRUE(watches.remove(id));
    EXPECT_FALSE(watches.remove(id));
    pipe.poke();
    EXPECT_EQ(round(watches), 0);
    EXPECT_EQ(calls, 0);
}

TEST(FdWatchTest, a_function_may_remove_another_watch_that_is_ready)
{
    FdWatch watches;
    const Pipe first;
    const Pipe second;
    int second_calls = 0;
    FdWatch::Id second_id = 0;
    watches.add(first.fds[0], [&] { watches.remove(second_id); });
    second_id = watches.add(second.fds[0], [&] { ++second_calls; });
    first.poke();
    second.poke();
    EXPECT_EQ(round(watches), 1);
    EXPECT_EQ(second_calls, 0);
}

TEST(FdWatchTest, a_function_may_remove_itself)
{
    FdWatch watches;
    const Pipe pipe;
    int calls = 0;
    FdWatch::Id id = 0;
    id = watches.add(pipe.fds[0], [&] {
        ++calls;
        watches.remove(id);
    });
    pipe.poke();
    EXPECT_EQ(round(watches), 1);
    EXPECT_EQ(round(watches), 0);
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(watches.empty());
}

TEST(FdWatchTest, the_end_of_the_file_is_a_reason_to_call)
{
    FdWatch watches;
    Pipe pipe;
    int calls = 0;
    watches.add(pipe.fds[0], [&] { ++calls; });
    close(pipe.fds[1]);
    pipe.fds[1] = dup(pipe.fds[0]); // so that the destructor closes a valid one
    EXPECT_EQ(round(watches), 1);
    EXPECT_EQ(calls, 1);
}
