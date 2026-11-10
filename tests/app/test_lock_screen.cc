#include "helpers.h"

#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "app/lock_screen.h"

using namespace tiny::app;

namespace {

std::vector<std::string> shell(const std::string& script)
{
    return {"/bin/sh", "-c", script};
}

//! One try of a program that asks and says the result "right" for "secret"
const char* const asks_for_secret =
        "echo 'PROMPT_HIDDEN Password:'; read line; "
        "if [ \"$line\" = 'ANSWER secret' ]; then echo 'RESULT OK'; "
        "else echo 'RESULT WRONG'; fi";

class LockScreenTest : public ::testing::Test {
  protected:
    WidgetFont font;

    //! Step until `done` is true (or 5 s are over)
    template <class Done>
    bool wait_for(LockScreen& screen, Done done)
    {
        const auto until =
                std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!done() && std::chrono::steady_clock::now() < until) {
            if (!screen.step(50)) {
                break;
            }
        }
        return done();
    }
};

} // namespace

TEST_F(LockScreenTest, it_covers_the_screen_with_a_window_no_manager_moves)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    const auto attributes = attributes_of(screen.window());
    EXPECT_TRUE(attributes.override_redirect);
    EXPECT_EQ(attributes.map_state, IsViewable);
    EXPECT_EQ(attributes.width,
              XDisplayWidth(tiny::get_display(),
                            XDefaultScreen(tiny::get_display())));
}

TEST_F(LockScreenTest, it_shows_what_the_check_asks)
{
    LockScreen screen(tiny::get_display(), shell(asks_for_secret), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    EXPECT_EQ(screen.prompt(), "Password:");
    EXPECT_TRUE(screen.entry().is_password());
}

TEST_F(LockScreenTest, a_visible_question_shows_the_answer)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'PROMPT_VISIBLE Code:'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    EXPECT_EQ(screen.prompt(), "Code:");
    EXPECT_FALSE(screen.entry().is_password());
}

TEST_F(LockScreenTest, information_of_the_check_is_shown)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'INFO Place your finger'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(
            screen, [&] { return screen.status() == "Place your finger"; }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, the_right_password_unlocks)
{
    LockScreen screen(tiny::get_display(), shell(asks_for_secret), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    screen.entry().set_text("secret");
    screen.submit();
    EXPECT_TRUE(
            screen.entry().get_text().empty()); // gone from the entry at once
    ASSERT_TRUE(wait_for(screen, [&] { return screen.unlocked(); }));
    EXPECT_EQ(attributes_of(screen.window()).map_state, IsUnmapped);
}

TEST_F(LockScreenTest, a_wrong_password_does_not_unlock_and_asks_again)
{
    LockScreen screen(tiny::get_display(), shell(asks_for_secret), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    screen.entry().set_text("nope");
    screen.submit();
    ASSERT_TRUE(wait_for(screen,
                         [&] { return screen.status() == "Wrong password"; }));
    EXPECT_FALSE(screen.unlocked());
    EXPECT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    // and the right one works after it
    screen.entry().set_text("secret");
    screen.submit();
    EXPECT_TRUE(wait_for(screen, [&] { return screen.unlocked(); }));
}

TEST_F(LockScreenTest, what_cant_be_checked_does_not_unlock)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'RESULT FAILED no PAM'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] {
        return screen.status() == "Can't check the password: no PAM";
    }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, a_check_that_ends_without_a_result_does_not_unlock)
{
    LockScreen screen(tiny::get_display(), shell("echo 'INFO bye'"), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] {
        return screen.status() == "The check ended without an answer";
    }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, a_result_that_is_not_understood_does_not_unlock)
{
    LockScreen screen(tiny::get_display(), shell("echo 'RESULT SURE'; sleep 5"),
                      "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(
            screen, [&] { return screen.status().find("Can't check") == 0; }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, a_check_that_can_not_be_started_does_not_unlock)
{
    LockScreen screen(tiny::get_display(), {}, "anna");
    ASSERT_TRUE(screen.lock());
    EXPECT_NE(screen.status().find("Can't check"), std::string::npos);
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, a_failed_try_is_tried_again_after_a_while)
{
    // every try of the program adds a line to the file
    const std::string tries =
            "/tmp/tiny-lock-tries-" + std::to_string(getpid());
    LockScreen screen(tiny::get_display(),
                      shell("echo x >> " + tries + "; echo 'RESULT FAILED x'"),
                      "anna");
    const auto count = [&] {
        std::ifstream in(tries);
        return std::count(std::istreambuf_iterator<char>(in),
                          std::istreambuf_iterator<char>(), '\n');
    };
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] { return screen.status() != ""; }));
    for (int i = 0; i < 5; ++i) { // not at once: a failure waits, no spinning
        screen.step(20);
    }
    EXPECT_EQ(count(), 1);
    EXPECT_TRUE(wait_for(screen, [&] { return count() >= 2; }));
    EXPECT_FALSE(screen.unlocked());
    std::remove(tries.c_str());
}

TEST_F(LockScreenTest, return_without_a_question_keeps_what_was_typed)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    screen.entry().set_text("typed early");
    screen.submit();
    EXPECT_EQ(screen.entry().get_text(), "typed early");
}

TEST_F(LockScreenTest, escape_forgets_the_text)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    screen.entry().set_text("typed");
    screen.entry().on_cancel(&screen.entry(), make_event(KeyPress));
    EXPECT_TRUE(screen.entry().get_text().empty());
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, typing_clears_what_was_said_about_the_last_try)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'INFO Wrong password'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    ASSERT_TRUE(wait_for(screen, [&] { return !screen.status().empty(); }));
    screen.entry().set_text("a");
    screen.entry().on_change(&screen.entry(), make_event(KeyPress));
    EXPECT_TRUE(screen.status().empty());
}

TEST_F(LockScreenTest, it_holds_the_keyboard_and_the_pointer)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());

    // another client can't take the keyboard now
    ::Display* other = XOpenDisplay(nullptr);
    ASSERT_NE(other, nullptr);
    EXPECT_EQ(XGrabKeyboard(other, XDefaultRootWindow(other), False,
                            GrabModeAsync, GrabModeAsync, CurrentTime),
              AlreadyGrabbed);
    EXPECT_EQ(XGrabPointer(other, XDefaultRootWindow(other), False,
                           ButtonPressMask, GrabModeAsync, GrabModeAsync, None,
                           None, CurrentTime),
              AlreadyGrabbed);
    XCloseDisplay(other);
}

TEST_F(LockScreenTest, the_grabs_end_with_the_lock)
{
    {
        LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
        ASSERT_TRUE(screen.lock());
    }
    ::Display* other = XOpenDisplay(nullptr);
    ASSERT_NE(other, nullptr);
    EXPECT_EQ(XGrabKeyboard(other, XDefaultRootWindow(other), False,
                            GrabModeAsync, GrabModeAsync, CurrentTime),
              GrabSuccess);
    XUngrabKeyboard(other, CurrentTime);
    XCloseDisplay(other);
}

TEST_F(LockScreenTest, a_grab_of_another_client_means_no_lock)
{
    ::Display* other = XOpenDisplay(nullptr);
    ASSERT_NE(other, nullptr);
    ASSERT_EQ(XGrabKeyboard(other, XDefaultRootWindow(other), False,
                            GrabModeAsync, GrabModeAsync, CurrentTime),
              GrabSuccess);
    XFlush(other);

    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    EXPECT_FALSE(screen.lock(std::chrono::milliseconds(150)));
    EXPECT_FALSE(screen.is_asking());
    XCloseDisplay(other);
}

TEST_F(LockScreenTest, a_window_over_the_lock_is_put_under_it)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    // a window that is over the lock, as a menu of another client could be
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True;
    const ::Window over = XCreateWindow(
            tiny::get_display(), root_window(), 0, 0, 50, 50, 0, CopyFromParent,
            InputOutput, CopyFromParent, CWOverrideRedirect, &attributes);
    XMapRaised(tiny::get_display(), over);
    XSync(tiny::get_display(), False);
    for (int i = 0; i < 20; ++i) {
        screen.step(20);
    }
    ::Window root;
    ::Window parent;
    ::Window* children = nullptr;
    unsigned count = 0;
    XQueryTree(tiny::get_display(), root_window(), &root, &parent, &children,
               &count);
    ASSERT_GT(count, 1u);
    EXPECT_EQ(children[count - 1], screen.window()); // the top one
    XFree(children);
    XDestroyWindow(tiny::get_display(), over);
}
