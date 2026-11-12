#include "helpers.h"

#include <X11/Xatom.h>

#include "wm/session_start.h"

namespace {

// The property of the root window belongs to the X server of the test run, so
// it is removed to start every test as a new session
class SessionStartTest : public ::testing::Test {
  protected:
    void SetUp() override { forget(); }
    void TearDown() override { forget(); }

    static void forget()
    {
        ::Display* display = tiny::get_display();
        XDeleteProperty(display, root_window(),
                        XInternAtom(display, "_TINY_SHELL_AUTOSTART", False));
        XSync(display, False);
    }
};

} // namespace

TEST_F(SessionStartTest, the_first_asking_gets_the_autostart)
{
    EXPECT_TRUE(tiny::wm::claim_autostart(tiny::get_display(), root_window()));
}

TEST_F(SessionStartTest, a_shell_that_starts_again_does_not_start_it_twice)
{
    EXPECT_TRUE(tiny::wm::claim_autostart(tiny::get_display(), root_window()));
    EXPECT_FALSE(tiny::wm::claim_autostart(tiny::get_display(), root_window()));
    EXPECT_FALSE(tiny::wm::claim_autostart(tiny::get_display(), root_window()));
}
