#include "helpers.h"

#include "wm_panel.h"

// ---- AppMenu ----

TEST(AppMenuTest, realize_creates_window_of_given_size)
{
    wm::AppMenu menu;

    menu.realize(root_window(), 5, 6);

    ASSERT_TRUE(menu.get_realized());
    XWindowAttributes attrs = attributes_of(menu);
    EXPECT_EQ(attrs.width, 10);
    EXPECT_EQ(attrs.height, 10);
    EXPECT_EQ(attrs.x, 5);
    EXPECT_EQ(attrs.y, 6);
}

// ---- UserMenu ----

TEST(UserMenuTest, realize_stacks_three_buttons_under_each_other)
{
    wm::UserMenu menu;

    menu.realize(root_window());

    ::Window top = child_at(menu.get_window(), 0, 0);
    ::Window middle = child_at(menu.get_window(), 0, 20);
    ::Window bottom = child_at(menu.get_window(), 0, 40);
    EXPECT_EQ(top, menu.restart.get_window());
    EXPECT_EQ(middle, menu.shutdown.get_window());
    EXPECT_EQ(bottom, menu.logout.get_window());
    EXPECT_EQ(children_of(menu.get_window()).size(), 3u);
}

TEST(UserMenuTest, buttons_have_labels)
{
    wm::UserMenu menu;

    EXPECT_EQ(menu.restart.get_text(), "Restart");
    EXPECT_EQ(menu.shutdown.get_text(), "Shutdown");
    EXPECT_EQ(menu.logout.get_text(), "Logout");
}

TEST(UserMenuTest, logout_click_is_signalled)
{
    WidgetFont font;
    wm::UserMenu menu;
    Recorder recorder;
    menu.logout.on_click.connect(&recorder, recorder.slot());
    menu.realize(root_window());

    send(menu.logout, EnterNotify);     // a click counts only on hover
    send(menu.logout, ButtonRelease);

    EXPECT_EQ(recorder.calls, 1);
    EXPECT_EQ(recorder.last_owner, &menu.logout);
}

// ---- Panel ----

TEST(PanelTest, realize_stretches_panel_over_the_root_window)
{
    wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    XWindowAttributes root = attributes_of(root_window());
    XWindowAttributes attrs = attributes_of(panel);
    EXPECT_EQ(attrs.width, root.width);
    EXPECT_EQ(attrs.height, (int) tiny::theme.wm_panel);
}

TEST(PanelTest, realize_puts_menu_buttons_to_both_ends)
{
    wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    XWindowAttributes root = attributes_of(root_window());
    ASSERT_EQ(children_of(panel.get_window()).size(), 2u);
    ::Window app = child_at(panel.get_window(), 0, 0);
    ASSERT_NE(app, 0u);
    EXPECT_EQ(attributes_of(app).width, 200);
    // the login button is aligned to the right end (Box keeps 1 px free)
    ::Window login = child_at(panel.get_window(), root.width - 121, 0);
    ASSERT_NE(login, 0u);
    EXPECT_EQ(attributes_of(login).width, 120);
}

TEST(PanelTest, realize_creates_menus_in_the_root_window)
{
    wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    EXPECT_TRUE(panel.menu_app.get_realized());
    EXPECT_TRUE(panel.menu_user.get_realized());
    EXPECT_EQ(panel.menu_app.get_parent_window(), root_window());
    EXPECT_EQ(panel.menu_user.get_parent_window(), root_window());
}

TEST(PanelTest, menus_stay_hidden_until_the_button_is_used)
{
    wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    EXPECT_EQ(attributes_of(panel.menu_app).map_state, IsUnmapped);
    EXPECT_EQ(attributes_of(panel.menu_user).map_state, IsUnmapped);
}

TEST(PanelTest, login_button_click_opens_the_user_menu)
{
    WidgetFont font;
    wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    panel.map_all();
    XWindowAttributes root = attributes_of(root_window());
    ::Window login = child_at(panel.get_window(), root.width - 121, 0);
    ASSERT_NE(login, 0u);

    send(login, make_event(EnterNotify));
    send(login, make_event(ButtonRelease));

    EXPECT_EQ(attributes_of(panel.menu_user).map_state, IsViewable);
}
