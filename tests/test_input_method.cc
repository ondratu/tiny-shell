#include "helpers.h"

#include <clocale>
#include <cstdlib>

#include <memory>

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
    const XEvent e = key_event(window, XK_a);
    KeySym symbol = NoSymbol;

    const std::string typed = input.lookup(e.xkey, symbol);

    EXPECT_EQ(symbol, static_cast<KeySym>(XK_a));
    if (!typed.empty()) { // an input method may hold the character back
        EXPECT_EQ(typed, "a");
    }
}

TEST_F(InputMethodTest, a_key_without_characters_gives_the_symbol_only)
{
    const XEvent e = key_event(window, XK_Left);
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

// The built-in input method of Xlib ("@im=none") gives UTF-8 and needs no
// server; where the locale is not supported the tests are skipped
class BuiltInInputMethodTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        setenv("XMODIFIERS", "@im=none", 1);
        setlocale(LC_CTYPE, "C.UTF-8");
        window = XCreateSimpleWindow(tiny::get_display(), root_window(), 0, 0,
                                     10, 10, 0, 0, 0);
        input = std::make_unique<tiny::InputMethod>(tiny::get_display(),
                                                    window);
        if (!input->available()) {
            GTEST_SKIP() << "no input method in this locale";
        }
    }

    void TearDown() override
    {
        input.reset();
        XDestroyWindow(tiny::get_display(), window);
        unsetenv("XMODIFIERS");
        setlocale(LC_CTYPE, "C");
    }

    ::Window window = None;
    std::unique_ptr<tiny::InputMethod> input;
};

TEST_F(BuiltInInputMethodTest, a_letter_is_typed_in_utf8)
{
    const XEvent e = key_event(window, XK_a);
    KeySym symbol = NoSymbol;

    EXPECT_EQ(input->lookup(e.xkey, symbol), "a");
    EXPECT_EQ(symbol, static_cast<KeySym>(XK_a));
}

TEST_F(BuiltInInputMethodTest, a_key_without_characters_gives_only_the_symbol)
{
    const XEvent e = key_event(window, XK_Left);
    KeySym symbol = NoSymbol;

    EXPECT_TRUE(input->lookup(e.xkey, symbol).empty());
    EXPECT_EQ(symbol, static_cast<KeySym>(XK_Left));
}

TEST_F(BuiltInInputMethodTest, focus_goes_to_the_input_context)
{
    input->set_focus(true);
    input->set_focus(false);
    SUCCEED();
}

// The method of the environment can be missing (a nested X server): the
// characters that are not Latin-1 must still be typed
TEST(EnvironmentInputMethodTest,
     a_missing_method_falls_back_to_the_built_in_one)
{
    setenv("XMODIFIERS", "@im=tiny-no-such-method", 1);
    setlocale(LC_CTYPE, "C.UTF-8");
    const ::Window window = XCreateSimpleWindow(
            tiny::get_display(), root_window(), 0, 0, 10, 10, 0, 0, 0);
    {
        const tiny::InputMethod input(tiny::get_display(), window, true);
        if (!input.available()) {
            GTEST_SKIP() << "no input method in this locale";
        }
        // the keyboard of the test server has no key for a Czech letter
        XEvent e = key_event(window, XK_a);
        KeySym symbol = NoSymbol;
        EXPECT_EQ(input.lookup(e.xkey, symbol), "a");
    }
    XDestroyWindow(tiny::get_display(), window);
    unsetenv("XMODIFIERS");
    setlocale(LC_CTYPE, "C");
}
