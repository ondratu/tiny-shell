#include "wm_helpers.h"
#include "fake_client.h"

#include <X11/Xatom.h>

#include "wm/panel.h"
#include "wm/tray.h"

using tiny::wm::Tray;

namespace {

//! Parent of a window, as the server says
::Window parent_of(::Window window)
{
    XSync(tiny::get_display(), False);
    ::Window the_root;
    ::Window parent = None;
    ::Window* children = nullptr;
    unsigned count = 0;
    XQueryTree(tiny::get_display(), window, &the_root, &parent, &children,
               &count);
    if (children) {
        XFree(children);
    }
    return parent;
}

} // namespace

class TrayTest : public ::testing::Test {
  protected:
    TrayTest()
    {
        panel.realize(root_window(), 0, 0);
        panel.map_all();
    }

    //! The request of a program to dock its icon
    void request_dock(::Window client)
    {
        XEvent e = make_event(ClientMessage);
        e.xclient.window = tray.get_window();
        e.xclient.message_type = tiny::get_display()._NET_SYSTEM_TRAY_OPCODE;
        e.xclient.format = 32;
        e.xclient.data.l[0] = CurrentTime;
        e.xclient.data.l[1] = 0; // REQUEST_DOCK
        e.xclient.data.l[2] = static_cast<long>(client);
        send(tray.get_window(), e);
    }

    void set_embed_info(FakeClient& client, long flags)
    {
        client.set_cardinals(tiny::get_display()._XEMBED_INFO, {0, flags});
    }

    //! The program changed its `_XEMBED_INFO`
    void notify_embed_info(::Window client)
    {
        XEvent e = make_event(PropertyNotify);
        e.xproperty.window = client;
        e.xproperty.atom = tiny::get_display()._XEMBED_INFO;
        send(client, e);
    }

    WidgetFont font;
    tiny::wm::Panel panel;
    Tray tray{panel, root_window(), 0};
};

TEST_F(TrayTest, becomes_the_owner_of_the_tray_selection)
{
    ASSERT_TRUE(tray.acquire());

    XSync(tiny::get_display(), False);
    const Atom selection =
            XInternAtom(tiny::get_display(),
                        tiny::wm::tray_selection_name(0).c_str(), False);
    EXPECT_EQ(XGetSelectionOwner(tiny::get_display(), selection),
              tray.get_window());
}

TEST_F(TrayTest, the_tray_is_horizontal)
{
    ASSERT_TRUE(tray.acquire());

    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(
                      tiny::get_display(), tray.get_window(),
                      tiny::get_display()._NET_SYSTEM_TRAY_ORIENTATION, 0, 1,
                      False, XA_CARDINAL, &type, &format, &count, &left, &data),
              Success);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(*reinterpret_cast<unsigned long*>(data), 0u);
    XFree(data);
}

TEST_F(TrayTest, another_tray_is_not_replaced)
{
    ASSERT_TRUE(tray.acquire());
    Tray other(panel, root_window(), 0);

    EXPECT_FALSE(other.acquire());
    EXPECT_EQ(tray.size(), 0u);
}

TEST_F(TrayTest, the_programs_are_told_with_the_manager_message)
{
    FakeClient program;
    XSelectInput(program.display, DefaultRootWindow(program.display),
                 StructureNotifyMask);
    program.sync();

    ASSERT_TRUE(tray.acquire());

    program.sync();
    XEvent e;
    bool told = false;
    while (XCheckTypedEvent(program.display, ClientMessage, &e)) {
        told = told || (e.xclient.message_type == tiny::get_display().MANAGER &&
                        static_cast<::Window>(e.xclient.data.l[2]) ==
                                tray.get_window());
    }
    EXPECT_TRUE(told);
}

TEST_F(TrayTest, a_request_embeds_the_window_in_the_panel)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program(0, 0, 40, 40);

    request_dock(program.window);

    ASSERT_EQ(tray.size(), 1u);
    const tiny::wm::TrayIcon* icon = tray.find(program.window);
    ASSERT_NE(icon, nullptr);
    EXPECT_EQ(parent_of(icon->get_window()), panel.get_window());
    EXPECT_EQ(parent_of(program.window), icon->get_window());
    const XWindowAttributes attrs = program.attrs();
    EXPECT_EQ(attrs.map_state, IsViewable);
    EXPECT_EQ(attrs.width, tiny::wm::shell_theme.tray_icon);
    EXPECT_EQ(attrs.height, tiny::wm::shell_theme.tray_icon);
}

TEST_F(TrayTest, the_icon_is_in_the_middle_of_its_place)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;

    request_dock(program.window);

    const auto icon = tiny::wm::shell_theme.tray_icon;
    EXPECT_EQ(attributes_of(*tray.find(program.window)).height,
              tiny::wm::shell_theme.panel);
    EXPECT_EQ(program.attrs().x, tiny::wm::shell_theme.tray_gap);
    EXPECT_EQ(program.attrs().y, (tiny::wm::shell_theme.panel - icon) / 2);
}

TEST_F(TrayTest, the_program_is_told_that_it_is_embedded)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    XEvent e;
    while (XCheckTypedEvent(program.display, ClientMessage, &e)) {}

    request_dock(program.window);

    program.sync();
    ASSERT_TRUE(XCheckTypedWindowEvent(program.display, program.window,
                                       ClientMessage, &e));
    EXPECT_EQ(e.xclient.message_type, tiny::get_display()._XEMBED);
    EXPECT_EQ(e.xclient.data.l[1], 0); // XEMBED_EMBEDDED_NOTIFY
    EXPECT_EQ(static_cast<::Window>(e.xclient.data.l[3]),
              tray.find(program.window)->get_window());
}

TEST_F(TrayTest, the_icons_follow_to_the_left)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient first;
    FakeClient second;

    request_dock(first.window);
    request_dock(second.window);

    ASSERT_EQ(tray.size(), 2u);
    EXPECT_LT(attributes_of(*tray.find(second.window)).x,
              attributes_of(*tray.find(first.window)).x);
}

TEST_F(TrayTest, a_window_is_docked_once)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;

    request_dock(program.window);
    request_dock(program.window);

    EXPECT_EQ(tray.size(), 1u);
}

TEST_F(TrayTest, a_window_that_is_not_there_is_ignored)
{
    ASSERT_TRUE(tray.acquire());
    ::Window gone;
    {
        FakeClient program;
        gone = program.window;
    }
    XSync(tiny::get_display(), False);

    request_dock(gone);
    request_dock(None);
    request_dock(root_window());

    EXPECT_EQ(tray.size(), 0u);
    EXPECT_FALSE(tray.has_icon(gone));
}

TEST_F(TrayTest, a_window_with_override_redirect_is_not_taken)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient menu;
    XSetWindowAttributes attrs = {};
    attrs.override_redirect = True;
    XChangeWindowAttributes(menu.display, menu.window, CWOverrideRedirect,
                            &attrs);
    menu.sync();

    request_dock(menu.window);

    EXPECT_FALSE(tray.has_icon(menu.window));
    EXPECT_EQ(parent_of(menu.window), root_window());
}

TEST_F(TrayTest, a_window_of_the_shell_is_not_taken)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient managed;
    tray.is_taken = [&](::Window w) { return w == managed.window; };

    request_dock(managed.window);

    EXPECT_EQ(tray.size(), 0u);
    EXPECT_EQ(parent_of(managed.window), root_window());
}

TEST_F(TrayTest, other_messages_are_ignored)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    XEvent e = make_event(ClientMessage);
    e.xclient.window = tray.get_window();
    e.xclient.message_type = tiny::get_display()._NET_SYSTEM_TRAY_OPCODE;
    e.xclient.format = 32;
    e.xclient.data.l[1] = 1; // BEGIN_MESSAGE
    e.xclient.data.l[2] = static_cast<long>(program.window);
    send(tray.get_window(), e);
    e.xclient.message_type = tiny::get_display().WM_PROTOCOLS;
    e.xclient.data.l[1] = 0;
    send(tray.get_window(), e);
    e.xclient.message_type = tiny::get_display()._NET_SYSTEM_TRAY_OPCODE;
    e.xclient.format = 8;
    send(tray.get_window(), e);

    EXPECT_EQ(tray.size(), 0u);
}

TEST_F(TrayTest, a_program_that_does_not_want_to_be_seen_takes_no_place)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    set_embed_info(program, 0);

    request_dock(program.window);

    EXPECT_EQ(tray.size(), 0u);
    EXPECT_TRUE(tray.has_icon(program.window));
    EXPECT_EQ(program.attrs().map_state, IsUnmapped);
}

TEST_F(TrayTest, a_program_shows_itself_with_the_embed_info)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    set_embed_info(program, 0);
    request_dock(program.window);

    set_embed_info(program, 1);
    notify_embed_info(program.window);

    EXPECT_EQ(tray.size(), 1u);
    EXPECT_EQ(program.attrs().map_state, IsViewable);
}

TEST_F(TrayTest, a_program_hides_itself_with_the_embed_info)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    set_embed_info(program, 1);
    request_dock(program.window);
    ASSERT_EQ(tray.size(), 1u);

    set_embed_info(program, 0);
    notify_embed_info(program.window);

    EXPECT_EQ(tray.size(), 0u);
    EXPECT_TRUE(tray.has_icon(program.window));
    EXPECT_EQ(parent_of(program.window), root_window());
    EXPECT_EQ(program.attrs().map_state, IsUnmapped);
}

TEST_F(TrayTest, hiding_an_icon_moves_the_others_back)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient first;
    FakeClient second;
    FakeClient third;
    request_dock(first.window);
    request_dock(second.window);
    request_dock(third.window);
    const int first_x = attributes_of(*tray.find(first.window)).x;
    const int third_x = attributes_of(*tray.find(third.window)).x;
    const int icon = tiny::wm::shell_theme.tray_icon +
                     2 * tiny::wm::shell_theme.tray_gap;

    set_embed_info(second, 0);
    notify_embed_info(second.window);

    EXPECT_EQ(attributes_of(*tray.find(first.window)).x, first_x);
    EXPECT_EQ(attributes_of(*tray.find(third.window)).x, third_x + icon);
}

TEST_F(TrayTest, other_changes_of_properties_are_ignored)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    request_dock(program.window);
    XEvent e = make_event(PropertyNotify);
    e.xproperty.window = program.window;
    e.xproperty.atom = XA_WM_NAME;

    send(program.window, e);

    EXPECT_EQ(tray.size(), 1u);
}

TEST_F(TrayTest, a_destroyed_window_is_forgotten)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient first;
    FakeClient second;
    request_dock(first.window);
    request_dock(second.window);
    const int first_x = attributes_of(*tray.find(first.window)).x;
    XDestroyWindow(second.display, second.window);
    second.sync();

    send(second.window, make_event(DestroyNotify));

    EXPECT_EQ(tray.size(), 1u);
    EXPECT_FALSE(tray.has_icon(second.window));
    EXPECT_EQ(attributes_of(*tray.find(first.window)).x, first_x);
}

TEST_F(TrayTest, a_window_that_is_taken_away_is_forgotten)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    request_dock(program.window);
    XReparentWindow(program.display, program.window,
                    DefaultRootWindow(program.display), 5, 5);
    program.sync();

    send(program.window, make_event(ReparentNotify));

    EXPECT_EQ(tray.size(), 0u);
    EXPECT_FALSE(tray.has_icon(program.window));
    EXPECT_EQ(parent_of(program.window), root_window()); // it is not touched
}

TEST_F(TrayTest, the_own_reparenting_is_not_taking_away)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    request_dock(program.window);

    send(program.window, make_event(ReparentNotify));

    EXPECT_EQ(tray.size(), 1u);
}

TEST_F(TrayTest, a_hidden_program_that_moves_is_not_forgotten)
{
    ASSERT_TRUE(tray.acquire());
    FakeClient program;
    set_embed_info(program, 0);
    request_dock(program.window);

    send(program.window, make_event(ReparentNotify));

    EXPECT_TRUE(tray.has_icon(program.window));
}

TEST_F(TrayTest, the_windows_go_back_to_the_root_window_with_the_tray)
{
    FakeClient program;
    const size_t children = children_of(panel.get_window()).size();
    {
        Tray other(panel, root_window(), 0);
        ASSERT_TRUE(other.acquire());
        XEvent e = make_event(ClientMessage);
        e.xclient.window = other.get_window();
        e.xclient.message_type = tiny::get_display()._NET_SYSTEM_TRAY_OPCODE;
        e.xclient.format = 32;
        e.xclient.data.l[2] = static_cast<long>(program.window);
        send(other.get_window(), e);
        ASSERT_EQ(other.size(), 1u);
        ASSERT_NE(parent_of(program.window), root_window());
    }

    EXPECT_EQ(parent_of(program.window), root_window());
    EXPECT_EQ(program.attrs().map_state, IsUnmapped);
    EXPECT_EQ(children_of(panel.get_window()).size(), children);
}

TEST_F(TrayTest, a_lost_selection_is_known)
{
    ASSERT_TRUE(tray.acquire());
    XSelectionClearEvent e = {};
    e.type = SelectionClear;
    e.window = tray.get_window();
    e.selection = XInternAtom(tiny::get_display(),
                              tiny::wm::tray_selection_name(0).c_str(), False);

    EXPECT_TRUE(tray.is_lost_by(e));
    e.selection = XA_PRIMARY;
    EXPECT_FALSE(tray.is_lost_by(e));
}
