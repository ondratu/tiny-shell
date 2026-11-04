#include "helpers.h"

#include "wm/panel.h"

// ---- MainMenu ----

TEST(MainMenuTest, realize_stacks_the_items_in_groups_with_lines_between)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    const int item = tiny::theme.wm_popup_item_height;
    const int gap = tiny::theme.wm_menu_separator;
    const ::Window win = menu.get_window();
    EXPECT_EQ(child_at(win, 0, 0), menu.settings.get_window());
    EXPECT_EQ(child_at(win, 0, item + gap), menu.applications.get_window());
    EXPECT_EQ(child_at(win, 0, 2 * item + gap), menu.terminal.get_window());
    EXPECT_EQ(child_at(win, 0, 3 * item + 2 * gap),
              menu.force_quit.get_window());
    EXPECT_EQ(child_at(win, 0, 4 * item + 3 * gap), menu.restart.get_window());
    EXPECT_EQ(child_at(win, 0, 5 * item + 3 * gap), menu.shutdown.get_window());
    EXPECT_EQ(child_at(win, 0, 6 * item + 4 * gap), menu.logout.get_window());
    EXPECT_EQ(children_of(win).size(), 11u); // the items and the lines
}

TEST(MainMenuTest, is_as_high_as_the_items_and_the_gaps)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    EXPECT_EQ(attributes_of(menu).height,
              7 * tiny::theme.wm_popup_item_height +
                      4 * tiny::theme.wm_menu_separator);
    EXPECT_EQ(attributes_of(menu).width, tiny::theme.wm_menu_width);
}

TEST(MainMenuTest, realize_twice_keeps_one_set_of_lines)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());
    menu.realize(root_window());

    EXPECT_EQ(children_of(menu.get_window()).size(), 11u);
}

TEST(MainMenuTest, buttons_have_labels)
{
    const tiny::wm::MainMenu menu;

    EXPECT_EQ(menu.settings.get_text(), "Settings");
    EXPECT_EQ(menu.applications.get_text(), "Applications");
    EXPECT_EQ(menu.terminal.get_text(), "Terminal");
    EXPECT_EQ(menu.force_quit.get_text(), "Force Quit");
    EXPECT_EQ(menu.restart.get_text(), "Restart");
    EXPECT_EQ(menu.shutdown.get_text(), "Shutdown");
    EXPECT_EQ(menu.logout.get_text(), "Logout");
}

TEST(MainMenuTest, a_click_on_an_item_is_signalled)
{
    const WidgetFont font;
    tiny::wm::MainMenu menu;
    Recorder recorder;
    menu.logout.on_click.connect(recorder.slot());
    menu.realize(root_window());

    send(menu.logout, EnterNotify); // a click counts only on hover
    send(menu.logout, ButtonRelease);

    EXPECT_EQ(recorder.calls, 1);
    EXPECT_EQ(recorder.last_owner, &menu.logout);
}

// ---- Panel ----

TEST(PanelTest, realize_stretches_panel_over_the_root_window)
{
    tiny::wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    const XWindowAttributes root = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(panel);
    EXPECT_EQ(attrs.width, root.width);
    EXPECT_EQ(attrs.height, tiny::theme.wm_panel);
}

TEST(PanelTest, realize_puts_the_main_menu_button_to_the_left)
{
    tiny::wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    ASSERT_EQ(children_of(panel.get_window()).size(), 1u);
    const ::Window main = child_at(panel.get_window(), 0, 0);
    ASSERT_NE(main, 0u);
    EXPECT_EQ(attributes_of(main).width, 200);
}

TEST(PanelTest, realize_creates_the_menu_in_the_root_window)
{
    tiny::wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    EXPECT_TRUE(panel.menu_main.get_realized());
    EXPECT_EQ(panel.menu_main.get_parent_window(), root_window());
}

TEST(PanelTest, the_menu_stays_hidden_until_the_button_is_used)
{
    tiny::wm::Panel panel;

    panel.realize(root_window(), 0, 0);

    EXPECT_EQ(attributes_of(panel.menu_main).map_state, IsUnmapped);
}

TEST(PanelTest, main_menu_button_click_opens_the_main_menu)
{
    const WidgetFont font;
    tiny::wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    panel.map_all();
    const ::Window main = child_at(panel.get_window(), 0, 0);
    ASSERT_NE(main, 0u);

    send(main, make_event(EnterNotify));
    send(main, make_event(ButtonRelease));

    EXPECT_EQ(attributes_of(panel.menu_main).map_state, IsViewable);
}

// ---- scaling (the scale of the theme) ----

class PanelScaledTest : public ::testing::Test {
  protected:
    void SetUp() override { tiny::theme.scale(2); }

    void TearDown() override { tiny::theme.scale(1); }
};

TEST_F(PanelScaledTest, main_menu_is_scaled)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    const XWindowAttributes attrs = attributes_of(menu);
    EXPECT_EQ(attrs.width, 400);
    EXPECT_EQ(attrs.height, 7 * 60 + 4 * 16);
}

TEST_F(PanelScaledTest, main_menu_buttons_are_scaled_and_stacked)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    EXPECT_EQ(attributes_of(menu.logout).width, 400);
    EXPECT_EQ(attributes_of(menu.logout).height, 60);
    EXPECT_EQ(child_at(menu.get_window(), 0, 4 * 60 + 3 * 16 + 60),
              menu.shutdown.get_window());
    EXPECT_EQ(child_at(menu.get_window(), 0, 6 * 60 + 4 * 16),
              menu.logout.get_window());
}

TEST_F(PanelScaledTest, panel_height_is_scaled)
{
    const tiny::wm::Panel panel;

    EXPECT_EQ(panel.get_height(), 60u);
}
