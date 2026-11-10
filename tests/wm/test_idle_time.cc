#include <gtest/gtest.h>

#include <unistd.h>

#include <X11/Xlib.h>

#include "wm/idle_time.h"

using tiny::wm::IdleTime;

namespace {

class IdleTimeTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        display = XOpenDisplay(nullptr);
        ASSERT_NE(display, nullptr);
    }

    void TearDown() override
    {
        if (display != nullptr) {
            XCloseDisplay(display);
        }
    }

    ::Display* display = nullptr;
};

} // namespace

TEST_F(IdleTimeTest, the_time_is_not_negative_and_is_read_again)
{
    const IdleTime idle(display);
    if (!idle.available()) {
        GTEST_SKIP() << "the X server has no IDLETIME counter";
    }
    const auto first = idle.idle();
    if (!first) {
        FAIL() << "the idle time can't be read";
    }
    EXPECT_GE(first->count(), 0);
    EXPECT_TRUE(idle.idle().has_value());
}

TEST_F(IdleTimeTest, the_idle_time_is_sane_a_while_later)
{
    const IdleTime idle(display);
    if (!idle.available()) {
        GTEST_SKIP() << "the X server has no IDLETIME counter";
    }
    const auto before = idle.idle();
    usleep(50 * 1000);
    const auto after = idle.idle();
    if (!before || !after) {
        FAIL() << "the idle time can't be read";
    }
    // a real input in between may reset it, so only a sane value is checked
    EXPECT_LT(after->count(), 1000LL * 3600 * 24 * 365);
}
