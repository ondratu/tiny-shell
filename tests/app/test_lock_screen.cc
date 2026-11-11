#include "helpers.h"

#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <thread>

#include "app/lock_screen.h"
#include "wm/idle_time.h"

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
    const LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    const auto attributes = attributes_of(screen.window());
    EXPECT_TRUE(attributes.override_redirect);
    EXPECT_EQ(attributes.map_state, IsViewable);
    EXPECT_EQ(attributes.width,
              XDisplayWidth(tiny::get_display(),
                            XDefaultScreen(tiny::get_display())));
}

TEST_F(LockScreenTest, a_change_of_the_screen_makes_the_window_cover_it_again)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    ::Display* const xdisplay = tiny::get_display();
    // what a screen that grew (a monitor was connected) looks like to the lock:
    // the window is smaller than the screen
    XResizeWindow(xdisplay, screen.window(), 10, 10);
    XSync(xdisplay, False);
    ASSERT_EQ(attributes_of(screen.window()).width, 10);

    screen.screen_changed();
    XSync(xdisplay, False);

    const auto attributes = attributes_of(screen.window());
    const int screen_number = XDefaultScreen(xdisplay);
    EXPECT_EQ(attributes.width, XDisplayWidth(xdisplay, screen_number));
    EXPECT_EQ(attributes.height, XDisplayHeight(xdisplay, screen_number));
    EXPECT_EQ(attributes.map_state, IsViewable);
}

TEST_F(LockScreenTest, it_shows_what_the_check_asks)
{
    LockScreen screen(tiny::get_display(), shell(asks_for_secret), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    EXPECT_EQ(screen.prompt(), "Password:");
    EXPECT_TRUE(screen.entry().is_password());
}

TEST_F(LockScreenTest, a_visible_question_shows_the_answer)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'PROMPT_VISIBLE Code:'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    EXPECT_EQ(screen.prompt(), "Code:");
    EXPECT_FALSE(screen.entry().is_password());
}

TEST_F(LockScreenTest, information_of_the_check_is_shown)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'INFO Place your finger'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    ASSERT_TRUE(wait_for(
            screen, [&] { return screen.status() == "Place your finger"; }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, the_right_password_unlocks)
{
    LockScreen screen(tiny::get_display(), shell(asks_for_secret), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
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
    screen.show_dialog();
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
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] {
        return screen.status() == "Can't check the password: no PAM";
    }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, a_check_that_ends_without_a_result_does_not_unlock)
{
    LockScreen screen(tiny::get_display(), shell("echo 'INFO bye'"), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
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
    screen.show_dialog();
    ASSERT_TRUE(wait_for(
            screen, [&] { return screen.status().find("Can't check") == 0; }));
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, a_check_that_can_not_be_started_does_not_unlock)
{
    LockScreen screen(tiny::get_display(), {}, "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
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
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] { return !screen.status().empty(); }));
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
    screen.show_dialog();
    screen.entry().set_text("typed early");
    screen.submit();
    EXPECT_EQ(screen.entry().get_text(), "typed early");
}

TEST_F(LockScreenTest, escape_forgets_the_text_and_closes_the_dialog)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    screen.show_dialog();
    screen.entry().set_text("typed");
    screen.entry().on_cancel(&screen.entry(), make_event(KeyPress));
    EXPECT_TRUE(screen.entry().get_text().empty());
    EXPECT_EQ(screen.state(), LockScreen::State::Idle); // back to the time
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, typing_clears_what_was_said_about_the_last_try)
{
    LockScreen screen(tiny::get_display(),
                      shell("echo 'INFO Wrong password'; sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] { return !screen.status().empty(); }));
    screen.entry().set_text("a");
    screen.entry().on_change(&screen.entry(), make_event(KeyPress));
    EXPECT_TRUE(screen.status().empty());
}

TEST_F(LockScreenTest, it_holds_the_keyboard_and_the_pointer)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();

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
        screen.show_dialog();
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
    screen.show_dialog();
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

TEST_F(LockScreenTest, an_answer_with_a_line_break_is_not_sent_changed)
{
    LockScreen screen(tiny::get_display(), shell(asks_for_secret), "anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    screen.entry().set_text("sec\nret");
    screen.submit();
    EXPECT_NE(screen.status().find("line break"), std::string::npos);
    EXPECT_TRUE(screen.is_asking()); // still asks, nothing was sent
    EXPECT_FALSE(screen.unlocked());
}

TEST_F(LockScreenTest, the_time_is_shown_in_big_letters_over_the_dialog)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna Nováková");
    // 14:05 on some day
    tm local = {};
    local.tm_year = 126;
    local.tm_mon = 5;
    local.tm_mday = 1;
    local.tm_hour = 14;
    local.tm_min = 5;
    local.tm_isdst = -1;

    screen.update_clock(mktime(&local));

    EXPECT_EQ(screen.clock_text(), "14:05");
}

TEST_F(LockScreenTest, the_time_is_there_from_the_start)
{
    const LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");

    EXPECT_EQ(screen.clock_text().size(), 5u);
    EXPECT_EQ(screen.clock_text()[2], ':');
}

// ---- what the screen shows and when

namespace {

XEvent key_event(int type = KeyPress, KeySym keysym = XK_a)
{
    XEvent event = {};
    event.type = type;
    event.xkey.keycode = XKeysymToKeycode(tiny::get_display(), keysym);
    return event;
}

XEvent pointer_event(int type)
{
    XEvent event = {};
    event.type = type;
    return event;
}

std::string marker_path(const char* name)
{
    return std::string("/tmp/tiny-lock-") + name + "-" +
           std::to_string(getpid());
}

//! A check that leaves a line in the file every time it is started
std::vector<std::string> counting_check(const std::string& file)
{
    return shell("echo x >> " + file +
                 "; echo 'PROMPT_HIDDEN Password:'; "
                 "sleep 5");
}

long lines_in(const std::string& file)
{
    std::ifstream in(file);
    return std::count(std::istreambuf_iterator<char>(in),
                      std::istreambuf_iterator<char>(), '\n');
}

} // namespace

TEST_F(LockScreenTest, it_starts_with_the_time_the_date_and_a_hint)
{
    const LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");

    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
    EXPECT_FALSE(screen.clock_text().empty());
    EXPECT_FALSE(screen.date_text().empty());
    EXPECT_FALSE(screen.is_asking());
}

TEST_F(LockScreenTest, the_date_is_of_the_format_in_the_options)
{
    LockOptions options;
    options.date_format = "%Y";
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    tm local = {};
    local.tm_year = 126; // 2026
    local.tm_mday = 1;
    local.tm_isdst = -1;

    screen.update_clock(mktime(&local));

    EXPECT_EQ(screen.date_text(), "2026");
}

TEST_F(LockScreenTest, an_empty_format_shows_no_date)
{
    LockOptions options;
    options.date_format = "";
    const LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna",
                            options);

    EXPECT_TRUE(screen.date_text().empty());
}

TEST_F(LockScreenTest, nothing_is_checked_until_somebody_is_there)
{
    const std::string file = marker_path("not-yet");
    std::remove(file.c_str());
    LockScreen screen(tiny::get_display(), counting_check(file), "Anna");
    ASSERT_TRUE(screen.lock());
    for (int i = 0; i < 5; ++i) {
        screen.step(20);
    }
    EXPECT_EQ(lines_in(file), 0);
    EXPECT_FALSE(screen.is_asking());

    screen.show_dialog();

    EXPECT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    EXPECT_EQ(lines_in(file), 1);
    std::remove(file.c_str());
}

TEST_F(LockScreenTest, a_key_opens_the_dialog_and_is_not_typed)
{
    LockScreen screen(tiny::get_display(), counting_check(marker_path("k")),
                      "Anna");
    XEvent event = key_event();

    EXPECT_TRUE(screen.handle_input(event));

    EXPECT_EQ(screen.state(), LockScreen::State::Dialog);
    EXPECT_TRUE(screen.entry().get_text().empty());
    std::remove(marker_path("k").c_str());
}

TEST_F(LockScreenTest, a_click_opens_the_dialog)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    XEvent event = pointer_event(ButtonPress);

    EXPECT_TRUE(screen.handle_input(event));

    EXPECT_EQ(screen.state(), LockScreen::State::Dialog);
}

TEST_F(LockScreenTest, moving_the_pointer_does_not_open_the_dialog)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    XEvent event = pointer_event(MotionNotify);

    EXPECT_TRUE(screen.handle_input(event));

    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
}

TEST_F(LockScreenTest, the_release_of_a_key_in_the_time_screen_is_used_up)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    XEvent release = key_event(KeyRelease);
    XEvent button = pointer_event(ButtonRelease);

    EXPECT_TRUE(screen.handle_input(release));
    EXPECT_TRUE(screen.handle_input(button));
    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
}

TEST_F(LockScreenTest, other_events_are_not_input)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    XEvent event = pointer_event(Expose);

    EXPECT_FALSE(screen.handle_input(event));
}

TEST_F(LockScreenTest, escape_closes_the_dialog_and_stops_the_check)
{
    const std::string file = marker_path("esc");
    std::remove(file.c_str());
    LockScreen screen(tiny::get_display(), counting_check(file), "Anna");
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    ASSERT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    screen.entry().set_text("half typed");

    screen.entry().on_cancel(&screen.entry(), make_event(KeyPress));

    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
    EXPECT_FALSE(screen.is_asking());
    EXPECT_TRUE(screen.entry().get_text().empty());
    EXPECT_TRUE(screen.status().empty());
    // and it asks again from the start when the dialog is opened again
    screen.show_dialog();
    EXPECT_TRUE(wait_for(screen, [&] { return screen.is_asking(); }));
    EXPECT_EQ(lines_in(file), 2);
    std::remove(file.c_str());
}

TEST_F(LockScreenTest, the_screen_goes_black_after_the_time)
{
    LockOptions options;
    options.blank_after = std::chrono::milliseconds(100);
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());

    ASSERT_TRUE(wait_for(screen, [&] {
        return screen.state() == LockScreen::State::Black;
    }));

    // nothing is on the screen: the time, the date and the hint are gone
    XSync(tiny::get_display(), False);
    EXPECT_EQ(attributes_of(screen.window()).map_state, IsViewable);
    ::Window root;
    ::Window parent;
    ::Window* children = nullptr;
    unsigned count = 0;
    XQueryTree(tiny::get_display(), screen.window(), &root, &parent, &children,
               &count);
    for (unsigned i = 0; i < count; ++i) {
        EXPECT_EQ(attributes_of(children[i]).map_state, IsUnmapped);
    }
    if (children != nullptr) {
        XFree(children);
    }
}

TEST_F(LockScreenTest, the_time_that_has_passed_without_input_counts_if_asked)
{
    const tiny::wm::IdleTime probe(tiny::get_display());
    if (!probe.available()) {
        GTEST_SKIP() << "the X server has no IDLETIME counter";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(700));
    const auto idle = probe.idle();
    if (!idle || *idle < std::chrono::milliseconds(700)) {
        GTEST_SKIP() << "there was input meanwhile";
    }
    LockOptions options;
    options.blank_after = std::chrono::milliseconds(1500);
    options.from_last_input = true;
    const auto start = std::chrono::steady_clock::now();
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());

    ASSERT_TRUE(wait_for(screen, [&] {
        return screen.state() == LockScreen::State::Black;
    }));
    // the 700 ms (at least) that have passed are not waited for again
    EXPECT_LT(std::chrono::steady_clock::now() - start,
              std::chrono::milliseconds(1300));
}

TEST_F(LockScreenTest, the_screen_is_not_black_early_when_the_time_is_not_asked)
{
    LockOptions options;
    options.blank_after = std::chrono::hours(1);
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());

    screen.step(100);
    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
}

TEST_F(LockScreenTest, input_wakes_a_black_screen_and_is_not_used_otherwise)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    screen.blank();
    ASSERT_EQ(screen.state(), LockScreen::State::Black);
    XEvent key = key_event();

    EXPECT_TRUE(screen.handle_input(key));

    EXPECT_EQ(screen.state(), LockScreen::State::Idle); // not the dialog yet
    screen.blank();
    XEvent motion = pointer_event(MotionNotify);
    EXPECT_TRUE(screen.handle_input(motion));
    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
    screen.blank();
    XEvent click = pointer_event(ButtonPress);
    EXPECT_TRUE(screen.handle_input(click));
    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
}

namespace {

//! The color of a pixel of the window of the lock
unsigned long pixel_of_cover(const LockScreen& screen)
{
    ::Display* const xdisplay = tiny::get_display();
    XSync(xdisplay, False);
    return Image(screen.window(), 4, 4).at(1, 1);
}

class LightTheme {
  public:
    LightTheme() { tiny::theme.set_variant(tiny::Variant::Light); }
    ~LightTheme() { tiny::theme.set_variant(tiny::Variant::Dark); }
};

} // namespace

TEST_F(LockScreenTest, the_dark_theme_has_a_black_background)
{
    const LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");

    EXPECT_EQ(pixel_of_cover(screen), 0x000000u);
}

TEST_F(LockScreenTest,
       the_light_theme_has_a_light_background_and_blank_is_black)
{
    const LightTheme light;
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    EXPECT_EQ(pixel_of_cover(screen), tiny::theme.widget.normal_bg);
    EXPECT_NE(tiny::theme.widget.normal_bg, 0x000000u);

    screen.blank(); // a blank screen is black in any theme, it saves power
    EXPECT_EQ(pixel_of_cover(screen), 0x000000u);

    screen.wake();
    EXPECT_EQ(pixel_of_cover(screen), tiny::theme.widget.normal_bg);
}

TEST_F(LockScreenTest, a_screen_with_a_dialog_does_not_go_black)
{
    LockOptions options;
    options.blank_after = std::chrono::milliseconds(50);
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();

    for (int i = 0; i < 10; ++i) {
        screen.step(20);
    }

    EXPECT_EQ(screen.state(), LockScreen::State::Dialog);
}

TEST_F(LockScreenTest, after_escape_the_time_to_the_black_screen_starts_again)
{
    LockOptions options;
    options.blank_after = std::chrono::milliseconds(150);
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());
    screen.show_dialog();
    for (int i = 0; i < 12; ++i) { // longer than the time
        screen.step(20);
    }
    ASSERT_EQ(screen.state(), LockScreen::State::Dialog);

    screen.hide_dialog();

    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
    EXPECT_TRUE(wait_for(screen, [&] {
        return screen.state() == LockScreen::State::Black;
    }));
}

TEST_F(LockScreenTest, a_move_of_the_pointer_delays_the_black_screen)
{
    LockOptions options;
    options.blank_after = std::chrono::milliseconds(200);
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());

    const auto start = std::chrono::steady_clock::now();
    XEvent motion = pointer_event(MotionNotify);
    while (std::chrono::steady_clock::now() - start <
           std::chrono::milliseconds(300)) {
        screen.handle_input(motion);
        screen.step(20);
    }

    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
}

TEST_F(LockScreenTest, a_screen_that_never_goes_black_stays_as_it_is)
{
    LockOptions options;
    options.blank_after = std::chrono::milliseconds(0);
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna", options);
    ASSERT_TRUE(screen.lock());

    for (int i = 0; i < 10; ++i) {
        screen.step(20);
    }

    EXPECT_EQ(screen.state(), LockScreen::State::Idle);
}

TEST_F(LockScreenTest, a_key_that_came_to_the_lock_is_given_to_the_entry)
{
    LockScreen screen(tiny::get_display(), shell("sleep 5"), "Anna");
    screen.show_dialog();
    XEvent event = key_event();
    event.xkey.window = screen.window(); // what the grab of the lock gives

    EXPECT_FALSE(screen.handle_input(event)); // for the handler of the entry

    EXPECT_EQ(event.xkey.window, screen.entry().get_window());
}

TEST_F(LockScreenTest, the_date_has_the_format_of_the_clock_without_the_time)
{
    EXPECT_EQ(date_format_of("%A %e %B %R"), "%A %e %B");
    EXPECT_EQ(date_format_of("%R %A %e %B"), "%A %e %B");
    EXPECT_EQ(date_format_of("%A, %d. %m. %Y, %H:%M:%S"), "%A, %d. %m. %Y");
    EXPECT_EQ(date_format_of("%d.%m. %-H:%M"), "%d.%m.");
    EXPECT_EQ(date_format_of("%a %e %b %l:%M %p"), "%a %e %b");
}

TEST_F(LockScreenTest, a_clock_with_nothing_but_the_time_gives_the_default_date)
{
    EXPECT_EQ(date_format_of("%R"), LockOptions().date_format);
    EXPECT_EQ(date_format_of(""), LockOptions().date_format);
    EXPECT_EQ(date_format_of("%H:%M:%S"), LockOptions().date_format);
}

TEST_F(LockScreenTest, a_date_in_the_middle_loses_the_time_but_not_its_parts)
{
    EXPECT_EQ(date_format_of("%d. %m. %Y"), "%d. %m. %Y");
    EXPECT_EQ(date_format_of("%a%%"), "%a%%");
}
