#include <gtest/gtest.h>
#include <unistd.h>

#include "app/lock_protocol.h"

using namespace tiny::app;

namespace {

void put(int fd, const std::string& text)
{
    ASSERT_EQ(write(fd, text.data(), text.size()),
              static_cast<ssize_t>(text.size()));
}

std::string drain(int fd)
{
    std::string all;
    char bytes[256];
    ssize_t got;
    while ((got = read(fd, bytes, sizeof bytes)) > 0) {
        all.append(bytes, static_cast<size_t>(got));
    }
    return all;
}

//! Two pipes for a LineDialog: what the lock says and what the auth says
struct Pipes {
    Pipes()
    {
        EXPECT_EQ(pipe(lock_to_auth), 0);
        EXPECT_EQ(pipe(auth_to_lock), 0);
    }
    ~Pipes()
    {
        for (const int fd : {lock_to_auth[0], lock_to_auth[1], auth_to_lock[0],
                             auth_to_lock[1]}) {
            close(fd);
        }
    }
    int lock_to_auth[2];
    int auth_to_lock[2];
};

} // namespace

TEST(LockProtocolTest, a_message_is_a_word_and_a_text_in_a_line)
{
    EXPECT_EQ(format_message(Kind::PromptHidden, "Password:"),
              "PROMPT_HIDDEN Password:\n");
    EXPECT_EQ(format_message(Kind::Cancel), "CANCEL\n");
    EXPECT_EQ(format_message(Kind::Answer, "a b"), "ANSWER a b\n");
}

TEST(LockProtocolTest, a_newline_in_the_text_is_a_space)
{
    EXPECT_EQ(format_message(Kind::Info, "one\ntwo\r\nthree"),
              "INFO one two  three\n");
}

TEST(LockProtocolTest, every_message_is_read_back)
{
    for (const Kind kind :
         {Kind::PromptHidden, Kind::PromptVisible, Kind::Info, Kind::Error,
          Kind::Result, Kind::Answer, Kind::Cancel}) {
        const auto message = parse_message(format_message(kind, "text here"));
        ASSERT_TRUE(message);
        EXPECT_EQ(message->kind, kind);
        EXPECT_EQ(message->text, "text here");
    }
}

TEST(LockProtocolTest, a_message_without_a_text_has_an_empty_one)
{
    const auto message = parse_message("CANCEL\n");
    ASSERT_TRUE(message);
    EXPECT_EQ(message->kind, Kind::Cancel);
    EXPECT_EQ(message->text, "");
}

TEST(LockProtocolTest, the_text_keeps_the_spaces)
{
    const auto message = parse_message("ANSWER  a  b ");
    ASSERT_TRUE(message);
    EXPECT_EQ(message->text, " a  b ");
}

TEST(LockProtocolTest, a_line_that_is_not_a_message_is_not_one)
{
    EXPECT_FALSE(parse_message(""));
    EXPECT_FALSE(parse_message("HELLO"));
    EXPECT_FALSE(parse_message("ANSWERED x")); // a longer word
    EXPECT_FALSE(parse_message("answer x"));
}

TEST(LockProtocolTest, the_outcome_is_told_in_the_result)
{
    EXPECT_EQ(result_message({Verdict::Right, ""}).text, "OK");
    EXPECT_EQ(result_message({Verdict::Wrong, "Authentication failure"}).text,
              "WRONG");
    EXPECT_EQ(result_message({Verdict::Failed, ""}).text, "FAILED");
    EXPECT_EQ(result_message({Verdict::Failed, "no PAM"}).text,
              "FAILED no PAM");
    EXPECT_EQ(result_message({Verdict::Failed, "x"}).kind, Kind::Result);
}

TEST(LockProtocolTest, the_result_is_read_back)
{
    EXPECT_EQ(outcome_of("OK").verdict, Verdict::Right);
    EXPECT_EQ(outcome_of("WRONG").verdict, Verdict::Wrong);
    const Outcome failed = outcome_of("FAILED no PAM");
    EXPECT_EQ(failed.verdict, Verdict::Failed);
    EXPECT_EQ(failed.text, "no PAM");
    EXPECT_EQ(outcome_of("FAILED").text, "");
}

TEST(LockProtocolTest, a_result_that_is_not_understood_never_unlocks)
{
    for (const char* text : {"", "ok", "OK ", "OKAY", "RIGHT", "OK WRONG"}) {
        EXPECT_EQ(outcome_of(text).verdict, Verdict::Failed) << text;
    }
}

TEST(LockProtocolTest, lines_are_given_when_they_are_whole)
{
    LineBuffer buffer;
    buffer.feed("ANSW", 4);
    EXPECT_FALSE(buffer.next());
    buffer.feed("ER x\nCAN", 8);
    EXPECT_EQ(buffer.next(), "ANSWER x");
    EXPECT_FALSE(buffer.next());
    buffer.feed("CEL\n\n", 5);
    EXPECT_EQ(buffer.next(), "CANCEL");
    EXPECT_EQ(buffer.next(), ""); // an empty line is a line
    EXPECT_FALSE(buffer.next());
    EXPECT_FALSE(buffer.overflow());
}

TEST(LockProtocolTest, a_line_that_is_too_long_is_dropped)
{
    LineBuffer buffer;
    const std::string long_line(LineBuffer::max_line + 1, 'a');
    buffer.feed(long_line.data(), long_line.size());
    EXPECT_FALSE(buffer.next()); // it does not end, and it is too long already
    EXPECT_TRUE(buffer.overflow());

    LineBuffer other;
    const std::string whole = long_line + "\nCANCEL\n";
    other.feed(whole.data(), whole.size());
    EXPECT_EQ(other.next(), "CANCEL"); // the long one is skipped
    EXPECT_TRUE(other.overflow());
}

TEST(LockProtocolTest, clear_forgets_what_is_waiting)
{
    LineBuffer buffer;
    buffer.feed("ANSWER x", 8);
    buffer.clear();
    buffer.feed("\n", 1);
    EXPECT_EQ(buffer.next(), "");
}

TEST(LockProtocolTest, the_dialog_asks_and_gets_the_answer)
{
    Pipes pipes;
    put(pipes.lock_to_auth[1], "ANSWER secret\n");
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_TRUE(dialog.ask(true, "Password:", answer));
    EXPECT_EQ(answer, "secret");
    close(pipes.auth_to_lock[1]);
    EXPECT_EQ(drain(pipes.auth_to_lock[0]), "PROMPT_HIDDEN Password:\n");
}

TEST(LockProtocolTest, a_visible_prompt_is_told_as_visible)
{
    Pipes pipes;
    put(pipes.lock_to_auth[1], "ANSWER 123\n");
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_TRUE(dialog.ask(false, "Code:", answer));
    close(pipes.auth_to_lock[1]);
    EXPECT_EQ(drain(pipes.auth_to_lock[0]), "PROMPT_VISIBLE Code:\n");
}

TEST(LockProtocolTest, the_dialog_skips_what_is_not_an_answer)
{
    Pipes pipes;
    put(pipes.lock_to_auth[1], "noise\nINFO hi\nANSWER x\n");
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_TRUE(dialog.ask(true, "P", answer));
    EXPECT_EQ(answer, "x");
}

TEST(LockProtocolTest, an_answer_may_come_in_pieces)
{
    Pipes pipes;
    put(pipes.lock_to_auth[1], "ANSWER sec");
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    const pid_t child = fork();
    if (child == 0) { // the rest comes later
        usleep(50000);
        put(pipes.lock_to_auth[1], "ret\n");
        _exit(0);
    }
    std::string answer;
    EXPECT_TRUE(dialog.ask(true, "P", answer));
    EXPECT_EQ(answer, "secret");
}

TEST(LockProtocolTest, cancel_is_no_answer)
{
    Pipes pipes;
    put(pipes.lock_to_auth[1], "CANCEL\n");
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_FALSE(dialog.ask(true, "P", answer));
}

TEST(LockProtocolTest, a_lock_that_is_gone_is_no_answer)
{
    Pipes pipes;
    close(pipes.lock_to_auth[1]);
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_FALSE(dialog.ask(true, "P", answer));
}

TEST(LockProtocolTest, a_line_that_does_not_end_is_no_answer)
{
    Pipes pipes;
    const std::string endless(LineBuffer::max_line + 10, 'a');
    put(pipes.lock_to_auth[1], endless);
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_FALSE(dialog.ask(true, "P", answer));
}

TEST(LockProtocolTest, nobody_to_write_to_is_no_answer)
{
    Pipes pipes;
    close(pipes.auth_to_lock[0]);
    signal(SIGPIPE, SIG_IGN);
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    std::string answer;
    EXPECT_FALSE(dialog.ask(true, "P", answer));
    dialog.say(false, "ignored"); // does not crash
    pipes.auth_to_lock[0] = -1;
}

TEST(LockProtocolTest, information_and_the_result_are_written)
{
    Pipes pipes;
    LineDialog dialog(pipes.lock_to_auth[0], pipes.auth_to_lock[1]);
    dialog.say(false, "Place your finger");
    dialog.say(true, "Too many tries");
    dialog.finish({Verdict::Wrong, ""});
    close(pipes.auth_to_lock[1]);
    EXPECT_EQ(drain(pipes.auth_to_lock[0]),
              "INFO Place your finger\nERROR Too many tries\nRESULT WRONG\n");
}
