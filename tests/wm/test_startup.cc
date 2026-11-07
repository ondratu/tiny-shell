#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

#include "wm/startup.h"

using tiny::wm::parse_startup_message;
using tiny::wm::StartupMessage;
using tiny::wm::StartupNotification;
using tiny::wm::StartupSequence;
using Clock = StartupNotification::Clock;

namespace {

// A text the way the protocol sends it: pieces of 20 bytes, the last one has
// the zero byte
std::vector<std::array<char, 20>> pieces_of(const std::string& text)
{
    const std::string with_zero = text + '\0';
    std::vector<std::array<char, 20>> pieces;
    for (size_t at = 0; at < with_zero.size(); at += 20) {
        std::array<char, 20> piece = {};
        std::memcpy(piece.data(), with_zero.data() + at,
                    std::min<size_t>(20, with_zero.size() - at));
        pieces.push_back(piece);
    }
    return pieces;
}

void send_text(StartupNotification& startup, ::Window sender,
               const std::string& text, Clock::time_point now)
{
    bool first = true;
    for (const auto& piece : pieces_of(text)) {
        startup.feed(sender, piece.data(), first, now);
        first = false;
    }
}

StartupSequence sequence(const std::string& id, const std::string& wmclass = "")
{
    StartupSequence s;
    s.id = id;
    s.wmclass = wmclass;
    return s;
}

} // namespace

TEST(StartupMessageTest, the_command_and_the_values_are_parsed)
{
    const StartupMessage message =
            parse_startup_message(
                    "new: ID=\"tiny_1\" NAME=Files SCREEN=0 ICON=folder")
                    .value_or(StartupMessage{});

    EXPECT_EQ(message.command, "new");
    EXPECT_EQ(message.values.at("ID"), "tiny_1");
    EXPECT_EQ(message.values.at("NAME"), "Files");
    EXPECT_EQ(message.values.at("ICON"), "folder");
}

TEST(StartupMessageTest, a_quoted_value_has_blanks_and_escapes)
{
    const StartupMessage message =
            parse_startup_message(
                    R"(change: ID="a b" NAME="say \"hi\" \\ now" BIN=x)")
                    .value_or(StartupMessage{});

    EXPECT_EQ(message.values.at("ID"), "a b");
    EXPECT_EQ(message.values.at("NAME"), "say \"hi\" \\ now");
    EXPECT_EQ(message.values.at("BIN"), "x");
}

TEST(StartupMessageTest, a_text_without_a_command_is_not_a_message)
{
    EXPECT_FALSE(parse_startup_message(""));
    EXPECT_FALSE(parse_startup_message("ID=1"));
    EXPECT_FALSE(parse_startup_message(": ID=1"));
}

TEST(StartupMessageTest, a_broken_tail_is_dropped)
{
    const StartupMessage message =
            parse_startup_message("remove: ID=1 word NAME")
                    .value_or(StartupMessage{});

    EXPECT_EQ(message.values.at("ID"), "1");
    EXPECT_EQ(message.values.size(), 1u);
}

TEST(StartupNotificationTest, a_message_in_several_pieces_starts_a_sequence)
{
    StartupNotification startup;
    const auto now = Clock::now();

    send_text(startup, 7,
              "new: ID=\"launcher-1_TIME5\" NAME=\"A long name of a program\" "
              "BIN=program WMCLASS=Program",
              now);

    ASSERT_EQ(startup.sequences().size(), 1u);
    const StartupSequence& s = startup.sequences().at("launcher-1_TIME5");
    EXPECT_EQ(s.name, "A long name of a program");
    EXPECT_EQ(s.bin, "program");
    EXPECT_EQ(s.wmclass, "Program");
    EXPECT_TRUE(startup.busy());
}

TEST(StartupNotificationTest, a_remove_message_ends_the_sequence)
{
    StartupNotification startup;
    const auto now = Clock::now();
    send_text(startup, 7, "new: ID=a", now);

    send_text(startup, 7, "remove: ID=a", now);

    EXPECT_TRUE(startup.sequences().empty());
    EXPECT_FALSE(startup.busy());
}

TEST(StartupNotificationTest, a_change_message_updates_a_known_sequence_only)
{
    StartupNotification startup;
    const auto now = Clock::now();
    send_text(startup, 7, "new: ID=a NAME=old", now);

    send_text(startup, 7, "change: ID=a NAME=new SILENT=1", now);
    send_text(startup, 7, "change: ID=b NAME=other", now);

    ASSERT_EQ(startup.sequences().size(), 1u);
    EXPECT_EQ(startup.sequences().at("a").name, "new");
    EXPECT_TRUE(startup.sequences().at("a").silent);
    EXPECT_FALSE(startup.busy()); // silent: nothing to show
}

TEST(StartupNotificationTest, a_piece_without_the_beginning_is_ignored)
{
    StartupNotification startup;
    char piece[20] = "ID=a";

    startup.feed(7, piece, false, Clock::now());

    EXPECT_TRUE(startup.sequences().empty());
}

TEST(StartupNotificationTest, messages_of_two_senders_do_not_mix)
{
    StartupNotification startup;
    const auto now = Clock::now();
    const auto a = pieces_of("new: ID=first NAME=\"A name that is long\"");
    const auto b = pieces_of("new: ID=second NAME=\"Another long name\"");

    // the pieces of both come one after another
    for (size_t i = 0; i < std::max(a.size(), b.size()); ++i) {
        if (i < a.size()) {
            startup.feed(1, a[i].data(), i == 0, now);
        }
        if (i < b.size()) {
            startup.feed(2, b[i].data(), i == 0, now);
        }
    }

    EXPECT_EQ(startup.sequences().size(), 2u);
    EXPECT_EQ(startup.sequences().at("second").name, "Another long name");
}

TEST(StartupNotificationTest, a_text_that_never_ends_is_dropped)
{
    StartupNotification startup;
    char piece[20];
    std::memset(piece, 'x', 20);

    startup.feed(7, piece, true, Clock::now());
    for (size_t i = 0; i < StartupNotification::longest_message / 20 + 2; ++i) {
        startup.feed(7, piece, false, Clock::now());
    }
    // the rest of the dropped text does not make a message
    send_text(startup, 8, "new: ID=ok", Clock::now());

    EXPECT_EQ(startup.sequences().size(), 1u);
}

TEST(StartupNotificationTest, a_sequence_ends_after_the_timeout)
{
    StartupNotification startup;
    const auto now = Clock::now();
    startup.begin(sequence("a"), now);

    EXPECT_FALSE(startup.expire(now + StartupNotification::timeout / 2));
    EXPECT_TRUE(startup.busy());
    EXPECT_EQ(startup.next_deadline().value_or(now),
              now + StartupNotification::timeout);

    EXPECT_TRUE(startup.expire(now + StartupNotification::timeout));
    EXPECT_FALSE(startup.busy());
    EXPECT_FALSE(startup.next_deadline());
}

TEST(StartupNotificationTest, a_window_of_the_class_ends_the_oldest_sequence)
{
    StartupNotification startup;
    const auto now = Clock::now();
    startup.begin(sequence("old", "Editor"), now);
    startup.begin(sequence("new", "Editor"), now + std::chrono::seconds(1));
    startup.begin(sequence("other", "Player"), now);

    EXPECT_TRUE(startup.finish_class("editor", "Editor"));

    EXPECT_FALSE(startup.sequences().count("old"));
    EXPECT_TRUE(startup.sequences().count("new"));
    EXPECT_FALSE(startup.finish_class("nothing", "Unknown"));
    EXPECT_FALSE(startup.finish_class("", ""));
}

TEST(StartupNotificationTest, finish_tells_whether_there_was_a_sequence)
{
    StartupNotification startup;
    startup.begin(sequence("a"), Clock::now());

    EXPECT_TRUE(startup.finish("a"));
    EXPECT_FALSE(startup.finish("a"));
}

TEST(StartupNotificationTest, the_number_of_sequences_is_limited)
{
    StartupNotification startup;
    const auto now = Clock::now();

    for (size_t i = 0; i < StartupNotification::most_sequences + 10; ++i) {
        startup.begin(sequence("s" + std::to_string(i)),
                      now + std::chrono::milliseconds(i));
    }

    EXPECT_EQ(startup.sequences().size(), StartupNotification::most_sequences);
    EXPECT_TRUE(startup.sequences().count(
            "s" + std::to_string(StartupNotification::most_sequences +
                                 9))); // the last
}

TEST(StartupNotificationTest, a_message_without_an_id_does_nothing)
{
    StartupNotification startup;

    send_text(startup, 7, "new: NAME=x", Clock::now());
    send_text(startup, 7, "remove: NAME=x", Clock::now());

    EXPECT_TRUE(startup.sequences().empty());
}

TEST(StartupNotificationTest, many_senders_that_do_not_finish_are_forgotten)
{
    StartupNotification startup;
    char piece[20];
    std::memset(piece, 'x', 20);

    for (::Window sender = 1; sender <= 40; ++sender) {
        startup.feed(sender, piece, true, Clock::now());
    }
    send_text(startup, 100, "new: ID=fine", Clock::now());

    EXPECT_TRUE(startup.sequences().count("fine"));
}
