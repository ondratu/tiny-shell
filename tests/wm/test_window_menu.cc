#include "fake_client.h"
#include "wm_helpers.h"

#include <memory>
#include <string>
#include <vector>

#include "wm/window.h"
#include "wm/window_menu.h"

using tiny::wm::WindowMenuAction;
using tiny::wm::WindowMenuState;

namespace {

using Actions = std::vector<WindowMenuAction>;

Actions actions_of(const WindowMenuState& state)
{
    Actions actions;
    for (const auto& item : tiny::wm::window_menu_items(state)) {
        actions.push_back(item.action);
    }
    return actions;
}

// A window that can do everything
WindowMenuState full()
{
    WindowMenuState state;
    state.can_maximize = state.can_minimize = state.can_move = true;
    state.can_resize = state.can_shade = state.can_close = true;
    return state;
}

} // namespace

TEST(WindowMenuItemsTest, a_normal_window_has_all_the_items_and_ends_with_close)
{
    EXPECT_EQ(actions_of(full()),
              (Actions{WindowMenuAction::Maximize, WindowMenuAction::Minimize,
                       WindowMenuAction::Move, WindowMenuAction::Resize,
                       WindowMenuAction::RollUp, WindowMenuAction::AlwaysOnTop,
                       WindowMenuAction::Close}));
}

TEST(WindowMenuItemsTest, the_labels_are_verbs_in_header_capitalization)
{
    std::vector<std::string> labels;
    for (const auto& item : tiny::wm::window_menu_items(full())) {
        labels.push_back(item.label);
    }
    EXPECT_EQ(labels, (std::vector<std::string>{"Maximize", "Minimize", "Move",
                                                "Resize", "Roll Up",
                                                "Always on Top", "Close"}));
}

TEST(WindowMenuItemsTest, a_maximized_window_can_be_restored_but_not_moved)
{
    WindowMenuState state = full();
    state.maximized = true;

    EXPECT_EQ(
            actions_of(state),
            (Actions{WindowMenuAction::Restore, WindowMenuAction::Minimize,
                     WindowMenuAction::AlwaysOnTop, WindowMenuAction::Close}));
}

TEST(WindowMenuItemsTest, a_shaded_window_rolls_down_and_has_no_resize)
{
    WindowMenuState state = full();
    state.shaded = true;

    EXPECT_EQ(
            actions_of(state),
            (Actions{WindowMenuAction::Minimize, WindowMenuAction::Move,
                     WindowMenuAction::RollDown, WindowMenuAction::AlwaysOnTop,
                     WindowMenuAction::Close}));
}

TEST(WindowMenuItemsTest, a_window_on_top_can_be_put_back)
{
    WindowMenuState state = full();
    state.above = true;

    const auto items = tiny::wm::window_menu_items(state);

    ASSERT_GE(items.size(), 2u);
    EXPECT_EQ(items[items.size() - 2].action, WindowMenuAction::NotAlwaysOnTop);
    EXPECT_EQ(items[items.size() - 2].label, "Not Always on Top");
}

TEST(WindowMenuItemsTest, a_fullscreen_window_can_only_be_minimized_or_closed)
{
    WindowMenuState state = full();
    state.fullscreen = true;

    EXPECT_EQ(actions_of(state),
              (Actions{WindowMenuAction::Minimize, WindowMenuAction::Close}));
}

TEST(WindowMenuItemsTest, what_the_window_does_not_allow_is_left_out)
{
    WindowMenuState state;
    state.can_close = true;
    state.can_move = true;

    EXPECT_EQ(actions_of(state),
              (Actions{WindowMenuAction::Move, WindowMenuAction::AlwaysOnTop,
                       WindowMenuAction::Close}));
}

TEST(WindowMenuItemsTest, a_window_that_cannot_be_closed_has_no_close)
{
    WindowMenuState state = full();
    state.can_close = false;

    EXPECT_NE(actions_of(state).back(), WindowMenuAction::Close);
}

// ---- the state of a window ----

class WindowMenuStateTest : public ::testing::Test {
  protected:
    FakeClient client;

    std::unique_ptr<tiny::wm::Window> manage(unsigned long functions = 0xFFFF)
    {
        return std::make_unique<tiny::wm::Window>(client.window, root_window(),
                                                  functions, screen_area());
    }
};

TEST_F(WindowMenuStateTest, follows_the_functions_of_the_window)
{
    const auto window = manage(tiny::wm::MotifWMHints::FUNC_MOVE |
                               tiny::wm::MotifWMHints::FUNC_CLOSE);

    const WindowMenuState state = tiny::wm::window_menu_state(*window);

    EXPECT_TRUE(state.can_move);
    EXPECT_TRUE(state.can_close);
    EXPECT_FALSE(state.can_resize);
    EXPECT_FALSE(state.can_maximize);
    EXPECT_FALSE(state.can_minimize);
}

TEST_F(WindowMenuStateTest, follows_the_state_of_the_window)
{
    const auto window = manage();
    window->set_above(true);

    EXPECT_TRUE(tiny::wm::window_menu_state(*window).above);
    EXPECT_FALSE(tiny::wm::window_menu_state(*window).maximized);
}

// ---- the menu ----

class WindowMenuTest : public WindowMenuStateTest {
  protected:
    WidgetFont font; // the text of the menu

    void SetUp() override
    {
        menu.realize(root_window());
        menu.on_action = [this](tiny::wm::Window& window,
                                WindowMenuAction action) {
            chosen_for = &window;
            chosen.push_back(action);
        };
    }

    std::string text_of(size_t index)
    {
        return menu.get_menu().item(index).get_text();
    }

    void click(size_t index)
    {
        send(menu.get_menu().item(index), EnterNotify);
        send(menu.get_menu().item(index), ButtonRelease);
        pump_events();
    }

    tiny::wm::WindowMenu menu;
    tiny::wm::Window* chosen_for = nullptr;
    Actions chosen;
};

TEST_F(WindowMenuTest, opens_with_the_items_of_the_window)
{
    const auto window = manage();

    menu.popup(*window, 50, 40);

    EXPECT_TRUE(menu.is_open());
    EXPECT_EQ(menu.target(), window.get());
    EXPECT_EQ(menu.get_menu().count(), 6u);
    EXPECT_EQ(text_of(0), "Maximize");
    EXPECT_EQ(text_of(5), "Close");
    EXPECT_EQ(attributes_of(menu.get_menu()).map_state, IsViewable);
}

TEST_F(WindowMenuTest, opens_below_the_pointer_and_stays_on_the_screen)
{
    const auto window = manage();

    menu.popup(*window, 50, 40);

    const XWindowAttributes at = attributes_of(menu.get_menu());
    EXPECT_EQ(at.y, 40);
    EXPECT_GE(at.x, 0);
}

TEST_F(WindowMenuTest, a_click_on_an_item_closes_the_menu_and_tells_the_window)
{
    const auto window = manage();
    menu.popup(*window, 50, 40);

    click(2); // Move

    EXPECT_EQ(chosen, (Actions{WindowMenuAction::Move}));
    EXPECT_EQ(chosen_for, window.get());
    EXPECT_FALSE(menu.is_open());
    EXPECT_EQ(menu.target(), nullptr);
}

TEST_F(WindowMenuTest, a_window_that_goes_away_closes_its_menu)
{
    const auto window = manage();
    menu.popup(*window, 50, 40);

    menu.forget(window.get());

    EXPECT_FALSE(menu.is_open());
    EXPECT_EQ(menu.target(), nullptr);
}

TEST_F(WindowMenuTest, another_window_going_away_leaves_the_menu_open)
{
    const auto window = manage();
    const FakeClient other_client;
    const tiny::wm::Window other(other_client.window, root_window(), 0xFFFF,
                                 screen_area());
    menu.popup(*window, 50, 40);

    menu.forget(&other);

    EXPECT_TRUE(menu.is_open());
}

TEST_F(WindowMenuTest, a_second_popup_replaces_the_first)
{
    const auto window = manage();
    const FakeClient other_client;
    tiny::wm::Window other(other_client.window, root_window(), 0xFFFF,
                           screen_area());
    menu.popup(*window, 50, 40);

    menu.popup(other, 60, 50);

    EXPECT_EQ(menu.target(), &other);
    EXPECT_TRUE(menu.is_open());
}
