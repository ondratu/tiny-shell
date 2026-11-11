#include "fake_client.h"
#include "wm_helpers.h"

#include <memory>

#include "wm/switcher.h"

using tiny::wm::Switcher;
using tiny::wm::SwitcherCell;
using WmWindow = tiny::wm::Window;
using Arrow = Switcher::Arrow;

namespace {

int border()
{
    return tiny::wm::shell_theme.dock_border;
}

} // namespace

// The windows, the latest first: a, b, a, c -> the applications a, b, c
class SwitcherTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        const char* apps[] = {"a.desktop", "b.desktop", "a.desktop",
                              "c.desktop"};
        for (const char* app : apps) {
            clients.push_back(std::make_unique<FakeClient>());
            clients.back()->set_title(std::string("Title of ") +
                                      std::to_string(windows.size()));
            clients.back()->map();
            windows.push_back(std::make_unique<WmWindow>(clients.back()->window,
                                                         root_window(), 0xFFFF,
                                                         screen_area()));
            windows.back()->set_app_id(app);
        }
        switcher.realize(root_window());
        switcher.app_name = [this](const WmWindow& window) {
            names.push_back(window.get_app_id());
            return window.get_app_id();
        };
        switcher.icon_of = [this](const WmWindow&, int size) {
            sizes.push_back(size);
            return SwitcherCell();
        };
    }

    std::vector<WmWindow*> all()
    {
        std::vector<WmWindow*> list;
        list.reserve(windows.size());
        for (const auto& window : windows) {
            list.push_back(window.get());
        }
        return list;
    }

    WmWindow* at(size_t i) { return windows[i].get(); }

    std::vector<std::unique_ptr<FakeClient>> clients;
    std::vector<std::unique_ptr<WmWindow>> windows;
    std::vector<std::string> names;
    std::vector<int> sizes;
    WidgetFont font;
    const ShellFonts shell_fonts;
    Switcher switcher;
};

TEST_F(SwitcherTest, shows_the_applications_once_and_starts_on_the_second)
{
    switcher.open(all(), false, false);

    ASSERT_TRUE(switcher.is_open());
    EXPECT_EQ(attributes_of(switcher.get_apps_panel()).map_state, IsViewable);
    EXPECT_EQ(switcher.get_apps_panel().get_count(), 3u);
    EXPECT_EQ(switcher.get_apps_panel().get_selected(), 1u);
    EXPECT_EQ(attributes_of(switcher.get_windows_panel()).map_state,
              IsUnmapped);
}

TEST_F(SwitcherTest, the_icons_of_the_applications_are_the_size_of_the_dock)
{
    switcher.open(all(), false, false);

    EXPECT_EQ(sizes, (std::vector<int>(3, tiny::wm::shell_theme.icon)));
    EXPECT_EQ(names, (std::vector<std::string>{"a.desktop", "b.desktop",
                                               "c.desktop"}));
}

TEST_F(SwitcherTest, the_panel_is_in_the_middle_of_the_screen)
{
    switcher.open(all(), false, false);

    const auto& panel = switcher.get_apps_panel();
    const auto attrs = attributes_of(panel);
    const auto root = attributes_of(root_window());
    EXPECT_EQ(attrs.x, root.width / 2 - panel.get_width() / 2);
    EXPECT_EQ(attrs.y, (root.height - panel.get_height()) / 2);
}

TEST_F(SwitcherTest, closing_gives_the_first_window_of_the_application)
{
    switcher.open(all(), false, false);

    EXPECT_EQ(switcher.close(), at(1));
    EXPECT_FALSE(switcher.is_open());
    EXPECT_EQ(attributes_of(switcher.get_apps_panel()).map_state, IsUnmapped);
}

TEST_F(SwitcherTest, tab_moves_the_cursor_and_wraps_around)
{
    switcher.open(all(), false, false);

    switcher.next_app(false);
    EXPECT_EQ(switcher.get_apps_panel().get_selected(), 2u);
    switcher.next_app(false);
    EXPECT_EQ(switcher.get_apps_panel().get_selected(), 0u);
    switcher.next_app(true);

    EXPECT_EQ(switcher.close(), at(3));
}

TEST_F(SwitcherTest,
       the_cursor_on_the_current_application_gives_the_first_window)
{
    switcher.open(all(), false, false);
    switcher.next_app(false);
    switcher.next_app(false);

    EXPECT_EQ(switcher.close(), at(0));
}

TEST_F(SwitcherTest, shift_alt_tab_starts_on_the_last_application)
{
    switcher.open(all(), false, true);

    EXPECT_EQ(switcher.get_apps_panel().get_selected(), 2u);
}

TEST_F(SwitcherTest, down_shows_the_windows_of_the_application_with_big_icons)
{
    switcher.open(all(), false, false);
    sizes.clear();

    switcher.arrow(Arrow::Down);

    const auto& panel = switcher.get_windows_panel();
    EXPECT_EQ(attributes_of(panel).map_state, IsViewable);
    EXPECT_EQ(panel.get_count(), 1u); // the application b has one window
    EXPECT_EQ(sizes,
              std::vector<int>{tiny::wm::shell_theme.switcher_window_icon});
    EXPECT_NE(switcher.get_model(), nullptr);
}

TEST_F(SwitcherTest,
       the_list_of_windows_is_under_the_applications_in_the_middle)
{
    switcher.open(all(), false, false);

    switcher.arrow(Arrow::Down);

    const auto apps = attributes_of(switcher.get_apps_panel());
    const auto list = attributes_of(switcher.get_windows_panel());
    EXPECT_GE(list.y, apps.y + apps.height);
    EXPECT_EQ(list.x + switcher.get_windows_panel().get_width() / 2,
              attributes_of(root_window()).width / 2);
}

TEST_F(SwitcherTest, the_arrows_choose_a_window_of_the_application)
{
    switcher.open(all(), false, false);
    switcher.arrow(Arrow::Left); // the application a (two windows)
    switcher.arrow(Arrow::Down);

    switcher.arrow(Arrow::Right);

    EXPECT_EQ(switcher.get_windows_panel().get_count(), 2u);
    EXPECT_EQ(switcher.get_windows_panel().get_selected(), 1u);
    EXPECT_EQ(switcher.close(), at(2));
}

TEST_F(SwitcherTest, up_hides_the_windows_again)
{
    switcher.open(all(), false, false);
    switcher.arrow(Arrow::Down);

    switcher.arrow(Arrow::Up);

    EXPECT_EQ(attributes_of(switcher.get_windows_panel()).map_state,
              IsUnmapped);
    EXPECT_EQ(switcher.close(), at(1));
}

TEST_F(SwitcherTest, tab_on_the_windows_shows_the_windows_of_the_next_app)
{
    switcher.open(all(), false, true); // the application c
    switcher.arrow(Arrow::Down);
    ASSERT_EQ(switcher.get_windows_panel().get_count(), 1u);

    switcher.next_app(false); // the application a, two windows

    EXPECT_EQ(attributes_of(switcher.get_windows_panel()).map_state,
              IsViewable);
    EXPECT_EQ(switcher.get_windows_panel().get_count(), 2u);
    EXPECT_EQ(switcher.get_windows_panel().get_selected(), 0u);
}

TEST_F(SwitcherTest, the_windows_of_an_application_are_listed_once)
{
    switcher.open(all(), false, false);
    sizes.clear();

    switcher.arrow(Arrow::Down);
    switcher.arrow(Arrow::Right); // the list stays, only the cursor moves
    switcher.arrow(Arrow::Left);

    EXPECT_EQ(sizes.size(), 1u);
}

TEST_F(SwitcherTest, alt_semicolon_starts_on_the_window_used_before)
{
    switcher.open(all(), true, false);

    EXPECT_EQ(attributes_of(switcher.get_windows_panel()).map_state,
              IsViewable);
    EXPECT_EQ(switcher.get_windows_panel().get_selected(), 1u);
    EXPECT_EQ(switcher.close(), at(2));
}

TEST_F(SwitcherTest, the_key_for_windows_goes_through_the_windows)
{
    switcher.open(all(), true, false);

    switcher.next_window(false);
    EXPECT_EQ(switcher.get_windows_panel().get_selected(), 0u);
    switcher.next_window(true);

    EXPECT_EQ(switcher.close(), at(2));
}

TEST_F(SwitcherTest, tab_in_the_switcher_for_windows_goes_to_the_applications)
{
    switcher.open(all(), true, false);

    switcher.next_app(false);

    EXPECT_EQ(switcher.get_apps_panel().get_selected(), 1u);
    EXPECT_EQ(switcher.get_windows_panel().get_count(), 1u);
    EXPECT_EQ(switcher.close(), at(1));
}

TEST_F(SwitcherTest, knows_the_windows_it_shows)
{
    switcher.open({at(0), at(1)}, false, false);

    EXPECT_TRUE(switcher.contains(at(1)));
    EXPECT_FALSE(switcher.contains(at(3)));
    switcher.close();
    EXPECT_FALSE(switcher.contains(at(1)));
}

TEST_F(SwitcherTest, without_windows_it_stays_closed)
{
    switcher.open({}, false, false);

    EXPECT_FALSE(switcher.is_open());
    EXPECT_EQ(attributes_of(switcher.get_apps_panel()).map_state, IsUnmapped);
}

TEST_F(SwitcherTest, a_closed_switcher_ignores_the_keys)
{
    switcher.next_app(false);
    switcher.arrow(Arrow::Down);
    switcher.next_window(false);

    EXPECT_EQ(switcher.close(), nullptr);
    EXPECT_EQ(switcher.get_model(), nullptr);
}

TEST_F(SwitcherTest, without_callbacks_the_cells_are_empty)
{
    switcher.app_name = nullptr;
    switcher.icon_of = nullptr;

    switcher.open(all(), false, false);
    switcher.arrow(Arrow::Down);

    EXPECT_EQ(switcher.get_apps_panel().get_count(), 3u);
    EXPECT_EQ(switcher.get_windows_panel().get_count(), 1u);
}

TEST_F(SwitcherTest, the_cells_have_the_names_the_titles_and_the_dots)
{
    switcher.open(all(), false, false);

    const auto& apps = switcher.get_apps_panel();
    EXPECT_EQ(apps.get_cell(0).text, "a.desktop");
    EXPECT_EQ(apps.get_cell(0).dots, 2u);
    EXPECT_EQ(apps.get_cell(1).dots, 1u);

    switcher.arrow(Arrow::Left);
    switcher.arrow(Arrow::Down);
    const auto& list = switcher.get_windows_panel();
    EXPECT_EQ(list.get_cell(0).text, "Title of 0");
    EXPECT_EQ(list.get_cell(1).text, "Title of 2");
    EXPECT_EQ(list.get_cell(0).dots, 0u); // a window has no dots
}

TEST_F(SwitcherTest,
       many_applications_do_not_make_a_panel_wider_than_the_screen)
{
    std::vector<std::unique_ptr<FakeClient>> more_clients;
    std::vector<std::unique_ptr<WmWindow>> more;
    std::vector<WmWindow*> list;
    const int count = attributes_of(root_window()).width /
                              tiny::wm::shell_theme.switcher_cell_width +
                      5;
    for (int i = 0; i < count; ++i) {
        more_clients.push_back(std::make_unique<FakeClient>());
        more_clients.back()->map();
        more.push_back(std::make_unique<WmWindow>(more_clients.back()->window,
                                                  root_window(), 0xFFFF,
                                                  screen_area()));
        more.back()->set_app_id("app" + std::to_string(i));
        list.push_back(more.back().get());
    }

    switcher.open(list, false, true);

    EXPECT_LE(switcher.get_apps_panel().get_width(),
              attributes_of(root_window()).width - 4 * border());
    EXPECT_EQ(switcher.get_apps_panel().get_selected(),
              static_cast<size_t>(count - 1));
    switcher.close();
}

TEST_F(SwitcherTest, does_not_open_before_it_is_realized)
{
    Switcher unrealized;

    unrealized.open(all(), false, false);

    EXPECT_FALSE(unrealized.is_open());
}

TEST_F(SwitcherTest, windows_of_one_class_without_an_entry_are_one_application)
{
    std::vector<std::unique_ptr<FakeClient>> more_clients;
    std::vector<std::unique_ptr<WmWindow>> more;
    std::vector<WmWindow*> list;
    for (int i = 0; i < 3; ++i) {
        more_clients.push_back(std::make_unique<FakeClient>());
        more_clients.back()->set_class("xterm", "XTerm");
        more_clients.back()->map();
        more.push_back(std::make_unique<WmWindow>(more_clients.back()->window,
                                                  root_window(), 0xFFFF,
                                                  screen_area()));
        list.push_back(more.back().get());
    }

    switcher.open(list, false, false);

    EXPECT_EQ(switcher.get_apps_panel().get_count(), 1u);
    EXPECT_EQ(switcher.get_apps_panel().get_cell(0).dots, 3u);
    switcher.close();
}
