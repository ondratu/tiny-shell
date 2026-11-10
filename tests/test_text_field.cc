#include <gtest/gtest.h>

#include <string>

#include "text_field.h"

using tiny::TextField;

namespace {

//! Every byte is 10 pixels wide
int ten_per_byte(const std::string& text)
{
    return static_cast<int>(text.size()) * 10;
}

} // namespace

TEST(TextFieldTest, starts_with_the_cursor_at_the_end)
{
    const TextField field("abc");

    EXPECT_EQ(field.text(), "abc");
    EXPECT_EQ(field.cursor(), 3u);
    EXPECT_TRUE(TextField().text().empty());
}

TEST(TextFieldTest, insert_puts_the_text_at_the_cursor)
{
    TextField field("ac");
    field.left();

    EXPECT_TRUE(field.insert("b"));

    EXPECT_EQ(field.text(), "abc");
    EXPECT_EQ(field.cursor(), 2u);
    EXPECT_FALSE(field.insert(""));
}

TEST(TextFieldTest, backspace_and_erase_take_whole_characters)
{
    TextField field("a\xC5\x99"
                    "b\xE2\x82\xAC"); // a, r with a caron, b, euro

    EXPECT_TRUE(field.backspace()); // the euro sign has 3 bytes
    EXPECT_EQ(field.text(), "a\xC5\x99"
                            "b");
    field.left();
    EXPECT_TRUE(field.backspace());
    EXPECT_EQ(field.text(), "ab");
    EXPECT_EQ(field.cursor(), 1u);
    field.home();
    EXPECT_FALSE(field.backspace());
    EXPECT_TRUE(field.erase());
    EXPECT_EQ(field.text(), "b");
    EXPECT_TRUE(field.erase());
    EXPECT_FALSE(field.erase());
    EXPECT_TRUE(field.text().empty());
}

TEST(TextFieldTest, erase_takes_a_character_of_more_bytes)
{
    TextField field("\xE2\x82\xAC"
                    "x");
    field.home();

    EXPECT_TRUE(field.erase());

    EXPECT_EQ(field.text(), "x");
}

TEST(TextFieldTest, the_cursor_moves_by_characters)
{
    TextField field("\xC5\x99"
                    "a");

    EXPECT_TRUE(field.left());
    EXPECT_EQ(field.cursor(), 2u);
    EXPECT_TRUE(field.left());
    EXPECT_EQ(field.cursor(), 0u);
    EXPECT_FALSE(field.left());
    EXPECT_TRUE(field.right());
    EXPECT_EQ(field.cursor(), 2u);
    EXPECT_TRUE(field.right());
    EXPECT_FALSE(field.right());
    EXPECT_TRUE(field.home());
    EXPECT_FALSE(field.home());
    EXPECT_TRUE(field.end());
    EXPECT_FALSE(field.end());
}

TEST(TextFieldTest, set_text_puts_the_cursor_at_the_end)
{
    TextField field("abc");
    field.home();

    field.set_text("hello");

    EXPECT_EQ(field.cursor(), 5u);
}

TEST(TextFieldTest, set_cursor_stays_at_the_start_of_a_character)
{
    TextField field("a\xC5\x99"
                    "b");

    field.set_cursor(2); // in the middle of the second character
    EXPECT_EQ(field.cursor(), 1u);
    field.set_cursor(3);
    EXPECT_EQ(field.cursor(), 3u);
    field.set_cursor(100); // behind the end
    EXPECT_EQ(field.cursor(), 4u);
    field.set_cursor(0);
    EXPECT_EQ(field.cursor(), 0u);
}

TEST(TextFieldTest, the_pointer_finds_the_nearest_border)
{
    const TextField field("abcd");

    EXPECT_EQ(field.offset_at(-5, ten_per_byte), 0u);
    EXPECT_EQ(field.offset_at(4, ten_per_byte), 0u);
    EXPECT_EQ(field.offset_at(6, ten_per_byte), 1u);
    EXPECT_EQ(field.offset_at(24, ten_per_byte), 2u);
    EXPECT_EQ(field.offset_at(26, ten_per_byte), 3u);
    EXPECT_EQ(field.offset_at(39, ten_per_byte), 4u);
    EXPECT_EQ(field.offset_at(500, ten_per_byte), 4u);
}

TEST(TextFieldTest, the_pointer_never_lands_inside_of_a_character)
{
    const TextField field("\xE2\x82\xAC"
                          "\xE2\x82\xAC"); // two euro signs

    for (int x = -5; x < 100; ++x) {
        const size_t offset = field.offset_at(x, ten_per_byte);
        EXPECT_TRUE(offset == 0 || offset == 3 || offset == 6) << x;
    }
}

TEST(TextFieldTest, without_a_measure_the_pointer_gives_the_end)
{
    EXPECT_EQ(TextField("abc").offset_at(0, nullptr), 3u);
    EXPECT_EQ(TextField().offset_at(10, ten_per_byte), 0u);
}

TEST(TextFieldTest, wipe_empties_the_text_and_overwrites_the_bytes)
{
    TextField field("secret");
    field.backspace();
    field.backspace(); // the removed bytes stay behind the end
    const char* bytes = field.text().data();

    field.wipe();

    EXPECT_EQ(field.text(), "");
    EXPECT_EQ(field.cursor(), 0u);
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(bytes[i], 0) << i; // the buffer is kept, only emptied
    }
}

TEST(TextFieldTest, reserve_keeps_the_buffer_while_the_text_grows)
{
    TextField field;
    field.reserve(64);
    field.insert("a");
    const char* bytes = field.text().data();

    for (int i = 0; i < 40; ++i) {
        field.insert("b");
    }

    EXPECT_EQ(field.text().data(), bytes);
}
