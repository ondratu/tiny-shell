#include "helpers.h"

#include "input_method.h"

TEST(Latin1Test, ascii_stays_and_high_bytes_become_utf8)
{
    EXPECT_EQ(tiny::latin1_to_utf8("abc", 3), "abc");
    EXPECT_EQ(tiny::latin1_to_utf8("\xE9", 1), "\xC3\xA9");
    EXPECT_EQ(tiny::latin1_to_utf8("a\xFF", 2), "a\xC3\xBF");
    EXPECT_EQ(tiny::latin1_to_utf8("", 0), "");
}

class InputMethodTest : public ::testing::Test {
  protected:
    ::Display* display = tiny::get_display();
    ::Window window =
            XCreateSimpleWindow(display, root_window(), 0, 0, 10, 10, 0, 0, 0);
    tiny::InputMethod input{display, window};

    ~InputMethodTest() override { XDestroyWindow(display, window); }
};

TEST_F(InputMethodTest, a_letter_gives_its_character_and_symbol)
{
    XEvent e = key_event(window, XK_a);
    KeySym symbol = NoSymbol;

    const std::string typed = input.lookup(e.xkey, symbol);

    EXPECT_EQ(symbol, static_cast<KeySym>(XK_a));
    if (!typed.empty()) { // an input method may hold the character back
        EXPECT_EQ(typed, "a");
    }
}

TEST_F(InputMethodTest, a_key_without_characters_gives_the_symbol_only)
{
    XEvent e = key_event(window, XK_Left);
    KeySym symbol = NoSymbol;

    EXPECT_TRUE(input.lookup(e.xkey, symbol).empty());
    EXPECT_EQ(symbol, static_cast<KeySym>(XK_Left));
}

TEST_F(InputMethodTest, focus_can_change_with_or_without_a_method)
{
    input.set_focus(true);
    input.set_focus(false);
    SUCCEED();
}
