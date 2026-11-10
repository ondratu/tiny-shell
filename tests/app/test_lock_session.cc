#include <gtest/gtest.h>
#include <poll.h>
#include <unistd.h>

#include "app/lock_session.h"

using namespace tiny::app;

namespace {

std::vector<std::string> shell(const std::string& script)
{
    return {"/bin/sh", "-c", script};
}

//! What the session says until it has said `count` messages or has ended
std::vector<Message> collect(AuthSession& session, size_t count)
{
    std::vector<Message> all;
    while (session.running() && all.size() < count) {
        pollfd wait = {session.descriptor(), POLLIN, 0};
        if (poll(&wait, 1, 5000) <= 0) {
            break;
        }
        for (Message& message : session.read()) {
            all.push_back(std::move(message));
        }
    }
    return all;
}

} // namespace

TEST(LockSessionTest, it_tells_what_the_program_says)
{
    AuthSession session(
            shell("echo 'PROMPT_HIDDEN Password:'; echo 'INFO hi'"));
    ASSERT_TRUE(session.start());
    EXPECT_TRUE(session.running());
    const auto messages = collect(session, 2);
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].kind, Kind::PromptHidden);
    EXPECT_EQ(messages[0].text, "Password:");
    EXPECT_EQ(messages[1].kind, Kind::Info);
}

TEST(LockSessionTest, the_answer_reaches_the_program)
{
    AuthSession session(shell("read line; echo \"INFO got $line\""));
    ASSERT_TRUE(session.start());
    EXPECT_TRUE(session.answer("secret"));
    const auto messages = collect(session, 1);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].text, "got ANSWER secret");
}

TEST(LockSessionTest, a_program_that_has_ended_is_not_running)
{
    AuthSession session(shell("echo 'RESULT OK'"));
    ASSERT_TRUE(session.start());
    collect(session, 100); // to its end
    EXPECT_FALSE(session.running());
    EXPECT_EQ(session.descriptor(), -1);
    EXPECT_TRUE(session.read().empty());
    EXPECT_FALSE(session.answer("x"));
}

TEST(LockSessionTest, lines_that_are_not_messages_are_dropped)
{
    AuthSession session(shell("echo hello; echo 'INFO real'"));
    ASSERT_TRUE(session.start());
    const auto messages = collect(session, 1);
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].text, "real");
}

TEST(LockSessionTest, a_program_that_does_not_exist_ends_at_once)
{
    AuthSession session({"/nonexistent/tiny-lock-auth"});
    ASSERT_TRUE(session.start()); // the failure comes as an end of the output
    EXPECT_TRUE(collect(session, 1).empty());
    EXPECT_FALSE(session.running());
}

TEST(LockSessionTest, no_program_is_not_started)
{
    AuthSession session({});
    EXPECT_FALSE(session.start());
    EXPECT_FALSE(session.error().empty());
}

TEST(LockSessionTest, a_program_that_writes_endlessly_is_ended)
{
    AuthSession session(
            shell("yes aaaaaaaaaaaaaaaaaaaaaaaaaaaa | tr -d '\\n'"));
    ASSERT_TRUE(session.start());
    collect(session, 1);
    EXPECT_FALSE(session.running());
}

TEST(LockSessionTest, stop_ends_the_program)
{
    AuthSession session(shell("sleep 30"));
    ASSERT_TRUE(session.start());
    session.stop();
    EXPECT_FALSE(session.running());
    session.stop(); // twice is nothing
}

TEST(LockSessionTest, it_can_be_started_again)
{
    AuthSession session(shell("echo 'INFO again'"));
    ASSERT_TRUE(session.start());
    collect(session, 1);
    ASSERT_TRUE(session.start());
    EXPECT_EQ(collect(session, 1).size(), 1u);
}

TEST(LockSessionTest, run_to_end_gives_the_exit_status)
{
    EXPECT_EQ(run_to_end(shell("exit 0")), 0);
    EXPECT_EQ(run_to_end(shell("exit 3")), 3);
    EXPECT_EQ(run_to_end(shell("kill -9 $$")), -1);
    EXPECT_EQ(run_to_end({"/nonexistent/program"}), 127);
    EXPECT_EQ(run_to_end({}), -1);
}

TEST(LockSessionTest, run_to_end_has_no_input)
{
    EXPECT_EQ(run_to_end(shell("[ -z \"$(cat)\" ]")), 0);
}
