#include <gtest/gtest.h>

#include <X11/Xlib.h>

#include "display_power.h"

using tiny::DisplayPower;
using std::chrono::seconds;

namespace {

class DisplayPowerTest : public ::testing::Test {
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

TEST_F(DisplayPowerTest, a_time_is_set_and_read_back)
{
    DisplayPower power(display);
    if (!power.available()) {
        GTEST_SKIP() << "the X server has no DPMS";
    }
    const bool was_enabled = power.enabled();
    const auto was = power.timeout();
    ASSERT_TRUE(power.set_timeout(seconds(600)));
    EXPECT_TRUE(power.enabled());
    EXPECT_EQ(power.timeout(), seconds(600));
    // put it back as it was
    if (was) {
        power.set_timeout(*was);
    }
    if (!was_enabled) {
        power.set_timeout(seconds(0));
    }
}

TEST_F(DisplayPowerTest, zero_switches_the_power_saving_off)
{
    DisplayPower power(display);
    if (!power.available()) {
        GTEST_SKIP() << "the X server has no DPMS";
    }
    const bool was_enabled = power.enabled();
    const auto was = power.timeout();
    ASSERT_TRUE(power.set_timeout(seconds(0)));
    EXPECT_FALSE(power.enabled());
    EXPECT_FALSE(power.timeout().has_value());
    if (was_enabled && was) {
        power.set_timeout(*was);
    }
}

TEST_F(DisplayPowerTest, a_very_long_time_is_cut_to_what_the_protocol_takes)
{
    DisplayPower power(display);
    if (!power.available()) {
        GTEST_SKIP() << "the X server has no DPMS";
    }
    const bool was_enabled = power.enabled();
    const auto was = power.timeout();
    ASSERT_TRUE(power.set_timeout(seconds(1000000)));
    EXPECT_EQ(power.timeout(), seconds(65535));
    if (was_enabled && was) {
        power.set_timeout(*was);
    } else {
        power.set_timeout(seconds(0));
    }
}

TEST_F(DisplayPowerTest, off_and_on_need_the_power_saving_switched_on)
{
    DisplayPower power(display);
    if (!power.available()) {
        // nothing is done, nothing fails
        EXPECT_FALSE(power.off());
        EXPECT_FALSE(power.on());
        EXPECT_FALSE(power.enabled());
        EXPECT_FALSE(power.set_timeout(seconds(60)));
        return;
    }
    const bool was_enabled = power.enabled();
    const auto was = power.timeout();
    power.set_timeout(seconds(0));
    EXPECT_FALSE(power.off()); // refused, not an error of X
    power.set_timeout(seconds(600));
    EXPECT_TRUE(power.off());
    EXPECT_TRUE(power.on());
    if (was_enabled && was) {
        power.set_timeout(*was);
    } else {
        power.set_timeout(seconds(0));
    }
}
