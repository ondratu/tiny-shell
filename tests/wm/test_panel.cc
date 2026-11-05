#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include "label.h"
#include "wm/panel.h"
#include "wm/symbolic_icons.h"

// ---- MainMenu ----

TEST(MainMenuTest, realize_stacks_the_items_in_groups_with_lines_between)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    const int item = tiny::wm::shell_theme.popup_item_height;
    const int gap = tiny::wm::shell_theme.menu_separator;
    const ::Window win = menu.get_window();
    EXPECT_EQ(child_at(win, 0, 0), menu.about.get_window());
    EXPECT_EQ(child_at(win, 0, item + gap), menu.settings.get_window());
    EXPECT_EQ(child_at(win, 0, 2 * item + 2 * gap),
              menu.applications.get_window());
    EXPECT_EQ(child_at(win, 0, 3 * item + 2 * gap), menu.terminal.get_window());
    EXPECT_EQ(child_at(win, 0, 4 * item + 3 * gap),
              menu.force_quit.get_window());
    EXPECT_EQ(child_at(win, 0, 5 * item + 4 * gap), menu.restart.get_window());
    EXPECT_EQ(child_at(win, 0, 6 * item + 4 * gap), menu.shutdown.get_window());
    EXPECT_EQ(child_at(win, 0, 7 * item + 5 * gap), menu.logout.get_window());
    EXPECT_EQ(children_of(win).size(), 13u); // the items and the lines
}

TEST(MainMenuTest, is_as_high_as_the_items_and_the_gaps)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    EXPECT_EQ(attributes_of(menu).height,
              8 * tiny::wm::shell_theme.popup_item_height +
                      5 * tiny::wm::shell_theme.menu_separator);
    EXPECT_EQ(attributes_of(menu).width, tiny::wm::shell_theme.menu_width);
}

TEST(MainMenuTest, realize_twice_keeps_one_set_of_lines)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());
    menu.realize(root_window());

    EXPECT_EQ(children_of(menu.get_window()).size(), 13u);
}

TEST(MainMenuTest, items_have_no_border)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    EXPECT_EQ(attributes_of(menu.settings).border_width, 0);
    EXPECT_EQ(attributes_of(menu.logout).border_width, 0);
    EXPECT_EQ(attributes_of(menu.logout).width,
              tiny::wm::shell_theme.menu_width);
}

TEST(MainMenuTest, buttons_have_labels)
{
    const tiny::wm::MainMenu menu;

    EXPECT_EQ(menu.about.get_text(), "About Tiny Shell");
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
    EXPECT_EQ(attrs.height, tiny::wm::shell_theme.panel);
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

// ---- applets ----

TEST(PanelAppletTest, an_applet_at_the_right_is_at_the_right_edge)
{
    tiny::wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    tiny::Label clock(80, 20, "12:00");

    panel.add_applet(&clock);

    EXPECT_TRUE(clock.get_realized());
    EXPECT_EQ(clock.get_parent_window(), panel.get_window());
    // a box ends one pixel before its width
    EXPECT_EQ(attributes_of(clock).x + 80 + 1, attributes_of(panel).width);
}

TEST(PanelAppletTest, the_next_applet_is_beside_the_first_one)
{
    tiny::wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    tiny::Label first(80, 20);
    tiny::Label second(50, 20);

    panel.add_applet(&first);
    panel.add_applet(&second);

    EXPECT_EQ(attributes_of(first).x + 80 + 1, attributes_of(panel).width);
    EXPECT_EQ(attributes_of(second).x + 50, attributes_of(first).x);
}

TEST(PanelAppletTest, an_applet_at_the_left_is_after_the_main_menu_button)
{
    const WidgetFont font;
    tiny::wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    tiny::Label label(60, 20);

    panel.add_applet(&label, tiny::wm::Panel::Side::Left);

    EXPECT_EQ(attributes_of(label).x, tiny::wm::shell_theme.panel_app_width);
}

TEST(PanelAppletTest, an_applet_of_a_shown_panel_is_shown)
{
    tiny::wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    panel.map_all();
    tiny::Label label(60, 20);

    panel.add_applet(&label);

    EXPECT_EQ(attributes_of(label).map_state, IsViewable);
}

TEST(PanelAppletTest, an_applet_needs_a_realized_panel)
{
    tiny::wm::Panel panel;
    tiny::Label label(60, 20);

    panel.add_applet(&label);

    EXPECT_FALSE(label.get_realized());
}

// ---- scaling (the scale of the theme) ----

class PanelScaledTest : public ::testing::Test {
  protected:
    void SetUp() override { tiny::wm::shell_theme.scale(2); }

    void TearDown() override { tiny::wm::shell_theme.scale(1); }
};

TEST_F(PanelScaledTest, main_menu_is_scaled)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    const XWindowAttributes attrs = attributes_of(menu);
    EXPECT_EQ(attrs.width, 400);
    EXPECT_EQ(attrs.height, 8 * 60 + 5 * 16);
}

TEST_F(PanelScaledTest, main_menu_buttons_are_scaled_and_stacked)
{
    tiny::wm::MainMenu menu;

    menu.realize(root_window());

    EXPECT_EQ(attributes_of(menu.logout).width, 400);
    EXPECT_EQ(attributes_of(menu.logout).height, 60);
    EXPECT_EQ(child_at(menu.get_window(), 0, 6 * 60 + 4 * 16),
              menu.shutdown.get_window());
    EXPECT_EQ(child_at(menu.get_window(), 0, 7 * 60 + 5 * 16),
              menu.logout.get_window());
}

TEST_F(PanelScaledTest, panel_height_is_scaled)
{
    const tiny::wm::Panel panel;

    EXPECT_EQ(panel.get_height(), 60u);
}

TEST(PanelAppletTest, a_null_or_added_applet_is_refused)
{
    tiny::wm::Panel panel;
    panel.realize(root_window(), 0, 0);
    tiny::Label label(60, 20);

    panel.add_applet(nullptr);
    panel.add_applet(&label);
    const int x = attributes_of(label).x;
    panel.add_applet(&label);

    EXPECT_EQ(attributes_of(label).x, x);
}

// ---- The logo ----

TEST(PanelTest, the_logo_is_a_square_button_in_the_color_of_the_text)
{
    const IconFiles files({"start-here"});
    tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                  {files.dir()});
    tiny::wm::Panel panel;
    panel.set_icons(&icons);

    panel.realize(root_window(), 0, 0);
    panel.map_all();

    const ::Window main = child_at(panel.get_window(), 0, 0);
    ASSERT_NE(main, 0u);
    const int side = tiny::wm::shell_theme.panel;
    EXPECT_EQ(attributes_of(main).width, side);
    send(main, make_event(Expose));
    const Image img(main, side, side);
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    EXPECT_EQ(img.at(side / 2, side / 2), tiny::theme.widget.get_fg(normal));
    EXPECT_EQ(img.at(0, 0), tiny::theme.widget.get_bg(normal));
}

TEST(PanelTest, without_the_logo_file_the_button_shows_the_text)
{
    tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                  {"/nonexistent"});
    tiny::wm::Panel panel;
    panel.set_icons(&icons);

    panel.realize(root_window(), 0, 0);

    EXPECT_EQ(attributes_of(child_at(panel.get_window(), 0, 0)).width, 200);
}
