#include "helpers.h"

#include <X11/Xatom.h>

#include <chrono>
#include <thread>

#include "wm/selection.h"

using namespace std::chrono_literals;
using tiny::wm::Selection;

namespace {

//! Another client of the server that plays the window manager we replace
class OtherManager {
  public:
    OtherManager():
        other(XOpenDisplay(nullptr)),
        screen(XDefaultScreen(other)),
        root(XRootWindow(other, screen)),
        selection(XInternAtom(other, tiny::wm::selection_name(screen).c_str(),
                              False)),
        window(XCreateSimpleWindow(other, root, 0, 0, 1, 1, 0, 0, 0))
    {}

    ~OtherManager() { XCloseDisplay(other); }

    void own()
    {
        XSetSelectionOwner(other, selection, window, CurrentTime);
        XSync(other, False);
    }

    void exit()
    {
        XDestroyWindow(other, window);
        XSync(other, False);
    }

    bool got_selection_clear()
    {
        XSync(other, False);
        XEvent e;
        return XCheckTypedEvent(other, SelectionClear, &e);
    }

    ::Display* other;
    int screen;
    ::Window root;
    Atom selection;
    ::Window window;
};

} // namespace

class SelectionTest : public ::testing::Test {
  protected:
    ::Display* display = tiny::get_display();
    const int screen = XDefaultScreen(display);
    const ::Window root = XRootWindow(display, screen);
    Atom name = XInternAtom(display, tiny::wm::selection_name(screen).c_str(),
                            False);

    ::Window owner_of_selection()
    {
        XSync(display, False);
        return XGetSelectionOwner(display, name);
    }
};

TEST(SelectionNameTest, is_named_by_the_screen)
{
    EXPECT_EQ(tiny::wm::selection_name(0), "WM_S0");
    EXPECT_EQ(tiny::wm::selection_name(2), "WM_S2");
}

TEST_F(SelectionTest, free_selection_is_taken)
{
    Selection selection(display, root, screen);

    EXPECT_EQ(selection.acquire(false, 100ms), Selection::Result::Acquired);
    EXPECT_NE(owner_of_selection(), None);
}

TEST_F(SelectionTest, selection_is_given_back_with_the_object)
{
    {
        Selection selection(display, root, screen);
        selection.acquire(false, 100ms);
    }

    EXPECT_EQ(owner_of_selection(), None);
}

TEST_F(SelectionTest, owned_selection_is_not_taken_without_replace)
{
    OtherManager old;
    old.own();
    Selection selection(display, root, screen);

    EXPECT_EQ(selection.acquire(false, 100ms), Selection::Result::Occupied);
    EXPECT_EQ(owner_of_selection(), old.window);
    EXPECT_FALSE(old.got_selection_clear());
}

TEST_F(SelectionTest, replace_waits_until_the_old_manager_is_gone)
{
    OtherManager old;
    old.own();
    Selection selection(display, root, screen);

    std::thread leaving([&old] {
        std::this_thread::sleep_for(100ms);
        old.exit();
    });
    const auto start = std::chrono::steady_clock::now();
    const auto rv = selection.acquire(true, 5000ms);
    const auto waited = std::chrono::steady_clock::now() - start;
    leaving.join();

    EXPECT_EQ(rv, Selection::Result::Acquired);
    EXPECT_GE(waited, 90ms);
    EXPECT_LT(waited, 4000ms);
    EXPECT_TRUE(old.got_selection_clear());
}

TEST_F(SelectionTest, replace_does_not_wait_for_a_window_that_is_gone)
{
    OtherManager old;
    old.own();
    old.exit(); // the owner is None again, the selection is free

    Selection selection(display, root, screen);

    EXPECT_EQ(selection.acquire(true, 100ms), Selection::Result::Acquired);
}

TEST_F(SelectionTest, replace_gives_up_when_the_old_manager_stays)
{
    OtherManager old;
    old.own();
    Selection selection(display, root, screen);

    EXPECT_EQ(selection.acquire(true, 50ms), Selection::Result::Timeout);
}

TEST_F(SelectionTest, the_old_manager_is_told_when_it_loses_the_selection)
{
    OtherManager old;
    old.own();
    Selection selection(display, root, screen);
    selection.acquire(true, 50ms);

    EXPECT_TRUE(old.got_selection_clear());
}

TEST_F(SelectionTest, announce_sends_manager_message_to_the_root)
{
    OtherManager watcher;
    XSelectInput(watcher.other, watcher.root, StructureNotifyMask);
    XSync(watcher.other, False);
    Selection selection(display, root, screen);
    selection.acquire(false, 100ms);

    selection.announce();
    XSync(display, False);
    XSync(watcher.other, False);

    XEvent e;
    ASSERT_TRUE(XCheckTypedEvent(watcher.other, ClientMessage, &e));
    EXPECT_EQ(e.xclient.message_type, tiny::get_display().MANAGER);
    EXPECT_EQ(static_cast<Atom>(e.xclient.data.l[1]), name);
    EXPECT_EQ(static_cast<::Window>(e.xclient.data.l[2]), owner_of_selection());
}

TEST_F(SelectionTest, only_its_own_selection_clear_means_it_lost)
{
    Selection selection(display, root, screen);
    selection.acquire(false, 100ms);

    XSelectionClearEvent e = {};
    e.selection = name;
    e.window = owner_of_selection();
    EXPECT_TRUE(selection.is_lost_by(e));

    e.selection = XA_PRIMARY;
    EXPECT_FALSE(selection.is_lost_by(e));

    e.selection = name;
    e.window = root;
    EXPECT_FALSE(selection.is_lost_by(e));
}
