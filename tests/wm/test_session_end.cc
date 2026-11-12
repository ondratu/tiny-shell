#include <gtest/gtest.h>

#include "wm/session_end.h"

using namespace tiny::wm;
using namespace std::chrono_literals;
using Clock = SessionEnd::Clock;

TEST(SessionEndTest, it_is_idle_until_the_user_is_asked)
{
    const SessionEnd end;

    EXPECT_TRUE(end.idle());
    EXPECT_EQ(end.seconds_left(), 0);
    EXPECT_FALSE(end.timed_out());
}

TEST(SessionEndTest, the_user_is_asked_for_a_minute)
{
    SessionEnd end;
    const auto now = Clock::now();

    EXPECT_TRUE(end.begin(SessionKind::Restart, now));

    EXPECT_EQ(end.phase(), SessionEnd::Phase::Confirm);
    EXPECT_EQ(end.kind(), SessionKind::Restart);
    EXPECT_EQ(end.seconds_left(now), 60);
    EXPECT_EQ(end.seconds_left(now + 1ms), 60); // rounded up
    EXPECT_EQ(end.seconds_left(now + 1s), 59);
    EXPECT_EQ(end.seconds_left(now + 59s + 500ms), 1);
    EXPECT_FALSE(end.timed_out(now + 59s));
}

TEST(SessionEndTest, nobody_answers_and_the_time_is_over)
{
    SessionEnd end;
    const auto now = Clock::now();
    end.begin(SessionKind::Logout, now);

    EXPECT_TRUE(end.timed_out(now + 60s));
    EXPECT_EQ(end.seconds_left(now + 60s), 0);
}

TEST(SessionEndTest, a_session_that_is_ending_is_not_asked_again)
{
    SessionEnd end;
    ASSERT_TRUE(end.begin(SessionKind::Logout));

    EXPECT_FALSE(end.begin(SessionKind::Shutdown));
    EXPECT_EQ(end.kind(), SessionKind::Logout);

    end.proceed();
    EXPECT_FALSE(end.begin(SessionKind::Shutdown));
}

TEST(SessionEndTest, the_applications_are_waited_for_without_a_time_limit)
{
    SessionEnd end;
    const auto now = Clock::now();
    end.begin(SessionKind::Logout, now);

    end.proceed();

    EXPECT_EQ(end.phase(), SessionEnd::Phase::Closing);
    EXPECT_FALSE(end.timed_out(now + 24h));
    EXPECT_EQ(end.seconds_left(now), 0);
}

TEST(SessionEndTest, the_user_can_change_their_mind_at_any_time)
{
    SessionEnd end;
    end.begin(SessionKind::Logout);
    end.cancel();
    EXPECT_TRUE(end.idle());

    end.begin(SessionKind::Shutdown);
    end.proceed();
    end.cancel();
    EXPECT_TRUE(end.idle());
    EXPECT_TRUE(end.begin(SessionKind::Restart));
}

TEST(SessionEndTest, proceeding_without_a_question_does_nothing)
{
    SessionEnd end;

    end.proceed();

    EXPECT_TRUE(end.idle());
}

TEST(SessionEndTextTest, every_way_of_ending_has_a_title_and_a_question)
{
    EXPECT_EQ(session_title(SessionKind::Logout), "Log Out");
    EXPECT_EQ(session_title(SessionKind::Restart), "Restart");
    EXPECT_EQ(session_title(SessionKind::Shutdown), "Shut Down");
    EXPECT_EQ(session_question(SessionKind::Logout, 60),
              "You will be logged out automatically in 60 seconds.");
    EXPECT_EQ(session_question(SessionKind::Restart, 30),
              "The computer will restart automatically in 30 seconds.");
    EXPECT_EQ(session_question(SessionKind::Shutdown, 2),
              "The computer will shut down automatically in 2 seconds.");
}

TEST(SessionEndTextTest, one_second_is_not_seconds)
{
    EXPECT_EQ(session_question(SessionKind::Logout, 1),
              "You will be logged out automatically in 1 second.");
}

TEST(SessionEndTextTest, the_button_of_a_session_that_waits_says_anyway)
{
    EXPECT_EQ(session_proceed_label(SessionKind::Logout, false), "Log Out");
    EXPECT_EQ(session_proceed_label(SessionKind::Logout, true),
              "Log Out Anyway");
    EXPECT_EQ(session_proceed_label(SessionKind::Shutdown, true),
              "Shut Down Anyway");
}

TEST(SessionEndTextTest, the_names_of_the_applications_that_are_waited_for)
{
    EXPECT_EQ(session_waiting({}), "Waiting for the applications to close.");
    EXPECT_EQ(session_waiting({"Firefox"}), "Waiting for Firefox to close.");
    EXPECT_EQ(session_waiting({"Firefox", "Terminal"}),
              "Waiting for Firefox and Terminal to close.");
    EXPECT_EQ(session_waiting({"A", "B", "C"}),
              "Waiting for A, B and C to close.");
    EXPECT_EQ(session_waiting({"A", "B", "C", "D"}),
              "Waiting for A, B, C and 1 more to close.");
    EXPECT_EQ(session_waiting({"A", "B", "C", "D", "E", "F"}),
              "Waiting for A, B, C and 3 more to close.");
}
