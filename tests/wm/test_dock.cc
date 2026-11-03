#include "fake_client.h"
#include "helpers.h"

#include <memory>

#include "image/pixmap.h"
#include "wm/dock.h"

using WmWindow = tiny::wm::Window;

namespace {

int icon_size()
{
    return tiny::theme.wm_icon;
}

int border()
{
    return tiny::theme.wm_dock_border;
}

uint32_t widget_bg(tiny::Style::State state)
{
    return tiny::theme.widget.get_bg(static_cast<uint8_t>(state));
}

bool has_handler(int type, ::Window window)
{
    return tiny::handlers.count({type, window}) == 1;
}

// ---- DockButton ----

class DockButtonTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        button.realize(root_window(), 0, 0);
        button.map_all();
    }

    tiny::wm::DockButton button;
};

TEST_F(DockButtonTest, is_a_square_without_border)
{
    EXPECT_EQ(button.get_width(), icon_size());
    EXPECT_EQ(button.get_height(), icon_size());
    EXPECT_EQ(button.get_border(), 0u);
}

TEST_F(DockButtonTest, connects_its_events)
{
    for (const int type : {Expose, EnterNotify, LeaveNotify, ButtonRelease}) {
        EXPECT_TRUE(has_handler(type, button.get_window())) << type;
    }
}

TEST_F(DockButtonTest, button_release_emits_click)
{
    Recorder r;
    button.on_click.connect(r.slot());

    send(button, ButtonRelease);

    EXPECT_EQ(r.calls, 1);
    EXPECT_EQ(r.last_owner, &button);
}

TEST_F(DockButtonTest, draws_grid_of_dots)
{
    const uint32_t bg = widget_bg(tiny::Style::State::Normal);
    send(button, Expose);

    const Image img(button.get_window(), icon_size(), icon_size());
    EXPECT_NE(img.count_not(bg, icon_size()), 0u);
    EXPECT_EQ(img.at(0, 0), bg);
}

TEST(DockButtonScaleTest, dots_are_scaled)
{
    tiny::theme.scale(2);
    {
        tiny::wm::DockButton button;
        button.realize(root_window(), 0, 0);
        button.map_all();
        send(button, Expose);

        const uint32_t fg = tiny::theme.widget.get_fg(
                static_cast<uint8_t>(tiny::Style::State::Normal));
        const Image img(button.get_window(), button.get_width(),
                        button.get_height());
        const int origin = 3 * (button.get_width() / 14);

        ASSERT_EQ(button.get_width(), 96u);
        // 10 px dot (5 px at scale 1): its far edge is still filled
        EXPECT_EQ(img.at(origin + 5, origin + 8), fg);
        EXPECT_EQ(img.at(origin + 8, origin + 5), fg);
    }
    tiny::theme.scale(1);
}

TEST_F(DockButtonTest, hover_changes_background)
{
    send(button, EnterNotify);
    const Image hover(button.get_window(), icon_size(), icon_size());

    send(button, LeaveNotify);
    const Image normal(button.get_window(), icon_size(), icon_size());

    EXPECT_EQ(hover.at(0, 0), widget_bg(tiny::Style::State::Hover));
    EXPECT_EQ(normal.at(0, 0), widget_bg(tiny::Style::State::Normal));
}

TEST(DockButtonLifeTest, destructor_disconnects_events)
{
    ::Window window;
    {
        tiny::wm::DockButton button;
        button.realize(root_window(), 0, 0);
        window = button.get_window();
    }
    for (const int type : {Expose, EnterNotify, LeaveNotify, ButtonRelease}) {
        EXPECT_FALSE(has_handler(type, window)) << type;
    }
}

TEST(DockButtonLifeTest, destructor_of_unrealized_button_is_safe)
{
    const tiny::wm::DockButton button;
}

// ---- DockIcon ----

class DockIconTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        client.map();
        task = std::make_unique<WmWindow>(client.window, root_window(), 0xFFFF,
                                          screen_area());
        icon = std::make_unique<tiny::wm::DockIcon>(task.get());
        icon->realize(root_window(), 0, 0);
        icon->map_all();
    }

    FakeClient client;
    std::unique_ptr<WmWindow> task;
    std::unique_ptr<tiny::wm::DockIcon> icon;
};

TEST_F(DockIconTest, click_focuses_the_task)
{
    ASSERT_FALSE(task->is_focused());
    Recorder r;
    icon->on_click.connect(r.slot());

    send(*icon, ButtonRelease);

    EXPECT_TRUE(task->is_focused());
    EXPECT_EQ(r.calls, 1);
}

TEST_F(DockIconTest, without_icon_draws_a_circle)
{
    const uint32_t bg = widget_bg(tiny::Style::State::Normal);
    send(*icon, Expose);

    const Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_NE(img.count_not(bg, icon_size()), 0u);
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2), bg); // hollow
}

TEST_F(DockIconTest, draws_the_icon_of_the_task)
{
    icon.reset();
    task.reset();
    client.set_icon_pixmap(icon_size(), false); // red
    task = std::make_unique<WmWindow>(client.window, root_window(), 0xFFFF,
                                      screen_area());
    icon = std::make_unique<tiny::wm::DockIcon>(task.get());
    icon->realize(root_window(), 0, 0);
    icon->map_all();
    ASSERT_NE(task->get_icon(), 0u);

    send(*icon, Expose);

    const Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2), 0xFF0000u);
    EXPECT_EQ(img.at(0, 0), 0xFF0000u);
}

TEST_F(DockIconTest, the_icon_of_the_application_is_preferred)
{
    icon.reset();
    task.reset();
    client.set_icon_pixmap(icon_size(), false); // red
    task = std::make_unique<WmWindow>(client.window, root_window(), 0xFFFF,
                                      screen_area());
    tiny::image::Image image;
    image.width = icon_size();
    image.height = icon_size();
    const auto side = static_cast<size_t>(icon_size());
    image.pixels.assign(side * side, 0xFF00FF00U); // green, opaque
    ::Display* x_display = tiny::get_display();
    const Pixmap pixmap =
            tiny::image::create_pixmap(x_display, root_window(), image);
    if (pixmap == None) {
        GTEST_SKIP() << "no ARGB32 format";
    }
    const Picture picture = tiny::image::create_argb_picture(x_display, pixmap);
    task->set_app_icon({picture, icon_size(), icon_size()});
    icon = std::make_unique<tiny::wm::DockIcon>(task.get());
    icon->realize(root_window(), 0, 0);
    icon->map_all();

    send(*icon, Expose);

    const Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2), 0x00FF00u);
    EXPECT_EQ(img.at(0, 0), 0x00FF00u);
    XRenderFreePicture(x_display, picture);
    XFreePixmap(x_display, pixmap);
}

TEST_F(DockIconTest, focused_task_is_drawn_selected)
{
    send(*icon, Expose);
    const Image before(icon->get_window(), icon_size(), icon_size());

    task->set_focus();
    send(*icon, Expose);
    const Image after(icon->get_window(), icon_size(), icon_size());

    EXPECT_EQ(before.at(0, 0), widget_bg(tiny::Style::State::Normal));
    EXPECT_EQ(after.at(0, 0), widget_bg(tiny::Style::State::Selected));
}

TEST_F(DockIconTest, a_window_that_demands_attention_gets_a_bar)
{
    send(*icon, Expose);
    const Image before(icon->get_window(), icon_size(), icon_size());

    task->set_demands_attention(true);
    send(*icon, Expose);
    const Image after(icon->get_window(), icon_size(), icon_size());

    EXPECT_NE(before.at(icon_size() / 2, icon_size() - 1),
              tiny::theme.wm_dock.attention);
    EXPECT_EQ(after.at(icon_size() / 2, icon_size() - 1),
              tiny::theme.wm_dock.attention);
}

class DockIconFavoriteTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        icon = std::make_unique<tiny::wm::DockIcon>(nullptr, "a.desktop",
                                                    app_icon());
        icon->realize(root_window(), 0, 0);
        icon->map_all();
    }

    // A green square the size of the dock icons; the picture of the icon is
    // freed with the fixture
    WmWindow::AppIcon app_icon()
    {
        tiny::image::Image image;
        image.width = icon_size();
        image.height = icon_size();
        const auto side = static_cast<size_t>(icon_size());
        image.pixels.assign(side * side, 0xFF00FF00U);
        ::Display* x_display = tiny::get_display();
        pixmap = tiny::image::create_pixmap(x_display, root_window(), image);
        if (pixmap == None) {
            return {};
        }
        picture = tiny::image::create_argb_picture(x_display, pixmap);
        return {picture, icon_size(), icon_size()};
    }

    void TearDown() override
    {
        icon.reset();
        if (pixmap != None) {
            XRenderFreePicture(tiny::get_display(), picture);
            XFreePixmap(tiny::get_display(), pixmap);
        }
    }

    Pixmap pixmap = None;
    Picture picture = None;
    std::unique_ptr<tiny::wm::DockIcon> icon;
};

TEST_F(DockIconFavoriteTest, has_no_window_and_knows_its_application)
{
    EXPECT_EQ(icon->get_task(), nullptr);
    EXPECT_EQ(icon->get_app_id(), "a.desktop");
}

TEST_F(DockIconFavoriteTest, the_left_button_only_tells_the_click)
{
    Recorder r;
    icon->on_click.connect(r.slot());

    send(*icon, ButtonRelease);

    EXPECT_EQ(r.calls, 1);
}

TEST_F(DockIconFavoriteTest, the_right_button_tells_the_menu_not_the_click)
{
    Recorder click;
    Recorder menu;
    icon->on_click.connect(click.slot());
    icon->on_menu.connect(menu.slot());
    XEvent e = make_event(ButtonRelease);
    e.xbutton.button = Button3;

    send(icon->get_window(), e);

    EXPECT_EQ(click.calls, 0);
    EXPECT_EQ(menu.calls, 1);
    EXPECT_EQ(menu.last_owner, icon.get());
}

TEST_F(DockIconFavoriteTest, draws_its_icon)
{
    if (pixmap == None) {
        GTEST_SKIP() << "no ARGB32 format";
    }

    send(*icon, Expose);

    const Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2), 0x00FF00u);
}

TEST_F(DockIconFavoriteTest, without_an_icon_draws_a_circle)
{
    tiny::wm::DockIcon bare(nullptr, "b.desktop");
    bare.realize(root_window(), 0, 0);
    bare.map_all();
    const uint32_t bg = widget_bg(tiny::Style::State::Normal);

    send(bare, Expose);

    const Image img(bare.get_window(), icon_size(), icon_size());
    EXPECT_NE(img.count_not(bg, icon_size()), 0u);
}

TEST_F(DockIconTest, takes_the_application_of_its_window)
{
    task->set_app_id("b.desktop");
    const tiny::wm::DockIcon again(task.get());

    EXPECT_EQ(again.get_app_id(), "b.desktop");
    EXPECT_EQ(again.get_task(), task.get());
}

TEST_F(DockIconTest, the_right_button_does_not_focus_the_window)
{
    Recorder menu;
    icon->on_menu.connect(menu.slot());
    XEvent e = make_event(ButtonRelease);
    e.xbutton.button = Button3;

    send(icon->get_window(), e);

    EXPECT_FALSE(task->is_focused());
    EXPECT_EQ(menu.calls, 1);
}

// ---- arrange_dock ----

using Strings = std::vector<std::string>;

TEST(ArrangeDockTest, nothing_is_empty)
{
    EXPECT_TRUE(tiny::wm::arrange_dock({}, {}).empty());
}

TEST(ArrangeDockTest, windows_without_favorites_stay_in_their_order)
{
    const auto slots = tiny::wm::arrange_dock({}, {"a", "", "b"});

    ASSERT_EQ(slots.size(), 3u);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(slots[static_cast<size_t>(i)].favorite, -1);
        EXPECT_EQ(slots[static_cast<size_t>(i)].task, i);
    }
}

TEST(ArrangeDockTest, favorites_that_do_not_run_come_first_and_have_no_window)
{
    const auto slots = tiny::wm::arrange_dock({"a", "b"}, {"c"});

    ASSERT_EQ(slots.size(), 3u);
    EXPECT_EQ(slots[0].favorite, 0);
    EXPECT_EQ(slots[0].task, -1);
    EXPECT_EQ(slots[1].favorite, 1);
    EXPECT_EQ(slots[1].task, -1);
    EXPECT_EQ(slots[2].favorite, -1);
    EXPECT_EQ(slots[2].task, 0);
}

TEST(ArrangeDockTest, a_window_takes_the_place_of_its_favorite)
{
    const auto slots = tiny::wm::arrange_dock({"a", "b"}, {"c", "b"});

    ASSERT_EQ(slots.size(), 3u);
    EXPECT_EQ(slots[0].task, -1); // a does not run
    EXPECT_EQ(slots[1].favorite, 1);
    EXPECT_EQ(slots[1].task, 1);
    EXPECT_EQ(slots[2].favorite, -1);
    EXPECT_EQ(slots[2].task, 0);
}

TEST(ArrangeDockTest, only_the_first_window_of_an_application_takes_the_place)
{
    const auto slots = tiny::wm::arrange_dock({"a"}, {"a", "a"});

    ASSERT_EQ(slots.size(), 2u);
    EXPECT_EQ(slots[0].favorite, 0);
    EXPECT_EQ(slots[0].task, 0);
    EXPECT_EQ(slots[1].favorite, -1);
    EXPECT_EQ(slots[1].task, 1);
}

TEST(ArrangeDockTest, a_window_of_unknown_application_never_takes_a_place)
{
    const auto slots = tiny::wm::arrange_dock({"a"}, {""});

    ASSERT_EQ(slots.size(), 2u);
    EXPECT_EQ(slots[0].task, -1);
    EXPECT_EQ(slots[1].task, 0);
}

// ---- Dock ----

class DockTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        dock.realize(root_window(), 0, 0);
        dock.set_events();
        dock.map_all();
    }

    void add(WmWindow* w)
    {
        dock.add(w);
        pump_events(); // ConfigureNotify updates the dock size
    }

    void remove(WmWindow* w)
    {
        dock.remove(w);
        pump_events();
    }

    WmWindow* manage(FakeClient& c)
    {
        c.map();
        windows.push_back(std::make_unique<WmWindow>(c.window, root_window(),
                                                     0xFFFF, screen_area()));
        return windows.back().get();
    }

    int task_width(int tasks) const
    {
        return (tasks + 1) * (icon_size() + border()) + border();
    }

    tiny::wm::Dock dock;
    std::vector<std::unique_ptr<WmWindow>> windows;
};

TEST_F(DockTest, starts_with_room_for_the_menu_button)
{
    EXPECT_EQ(dock.get_width(), icon_size() + border() * 2);
    EXPECT_EQ(dock.get_height(), icon_size() + border() * 2);
    EXPECT_EQ(dock.get_style().padding_width, 5u);
}

TEST_F(DockTest, menu_button_is_in_the_dock)
{
    EXPECT_EQ(children_of(dock.get_window()).size(), 1u);
}

TEST_F(DockTest, is_placed_where_it_was_realized)
{
    auto attrs = attributes_of(dock);

    EXPECT_EQ(attrs.x, 0);
    EXPECT_EQ(attrs.y, 0);
}

TEST_F(DockTest, add_creates_an_icon_and_grows_the_dock)
{
    FakeClient c;
    add(manage(c));

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(attributes_of(dock).width, task_width(1));
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockTest, icons_are_placed_one_after_another)
{
    FakeClient c1;
    FakeClient c2;
    add(manage(c1));
    add(manage(c2));

    EXPECT_NE(child_at(dock.get_window(), border(), border()), 0u);
    EXPECT_NE(child_at(dock.get_window(), 2 * border() + icon_size(), border()),
              0u);
}

TEST_F(DockTest, dock_stays_centered_when_it_grows)
{
    FakeClient c1;
    FakeClient c2;
    add(manage(c1));
    add(manage(c2));

    auto root = attributes_of(root_window());
    auto attrs = attributes_of(dock);
    EXPECT_EQ(attrs.x, (root.width / 2 - dock.get_width() / 2));
    EXPECT_EQ(attrs.y, ((root.height - dock.get_height() - border())));
}

TEST_F(DockTest, remove_deletes_the_icon_and_shrinks_the_dock)
{
    FakeClient c1;
    FakeClient c2;
    WmWindow* w1 = manage(c1);
    WmWindow* w2 = manage(c2);
    add(w1);
    add(w2);

    remove(w1);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockTest, add_after_remove_gives_the_same_size_as_before)
{
    FakeClient c1;
    FakeClient c2;
    WmWindow* w1 = manage(c1);
    WmWindow* w2 = manage(c2);
    add(w1);
    add(w2);
    remove(w1);
    remove(w2);

    EXPECT_EQ(dock.get_width(), task_width(0));
}

TEST_F(DockTest, remove_of_unknown_window_changes_nothing)
{
    FakeClient c;
    WmWindow* w = manage(c);
    add(w);
    FakeClient other;
    WmWindow* unknown = manage(other);

    remove(unknown);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockTest, redraw_draws_the_icon_of_the_window)
{
    FakeClient c;
    WmWindow* w = manage(c);
    add(w);
    w->set_focus();

    dock.redraw(w, make_event(Expose));

    const ::Window icon = child_at(dock.get_window(), border(), border());
    ASSERT_NE(icon, 0u);
    const Image img(icon, icon_size(), icon_size());
    EXPECT_EQ(img.at(0, 0), widget_bg(tiny::Style::State::Selected));
}

TEST_F(DockTest, redraw_of_unknown_window_is_ignored)
{
    FakeClient c;
    const WmWindow* w = manage(c);
    const XErrors errors;

    dock.redraw(w, make_event(Expose));

    EXPECT_EQ(errors.errors(), 0);
}

TEST_F(DockTest, clicking_an_icon_focuses_its_window)
{
    FakeClient c;
    WmWindow* w = manage(c);
    add(w);
    const ::Window icon = child_at(dock.get_window(), border(), border());
    ASSERT_NE(icon, 0u);

    send(icon, make_event(ButtonRelease));

    EXPECT_TRUE(w->is_focused());
}

// ---- favorites ----

class DockFavoritesTest : public DockTest {
  protected:
    WidgetFont font; // the text of the menu

    static std::vector<tiny::wm::DockFavorite>
    favorites(const std::vector<std::string>& ids)
    {
        std::vector<tiny::wm::DockFavorite> list;
        list.reserve(ids.size());
        for (const std::string& id : ids) {
            list.push_back({id, {}});
        }
        return list;
    }

    void set(const std::vector<std::string>& ids)
    {
        dock.set_favorites(favorites(ids));
        pump_events();
    }

    // The window of the icon at the place `index`
    ::Window icon_at(int index) const
    {
        return child_at(dock.get_window(),
                        border() + index * (icon_size() + border()), border());
    }

    void right_click(::Window icon)
    {
        XEvent e = make_event(ButtonRelease);
        e.xbutton.button = Button3;
        e.xbutton.y_root = 500;
        e.xbutton.y = 10;
        e.xbutton.x_root = 300;
        send(icon, e);
        pump_events();
    }

    WmWindow* manage_app(FakeClient& c, const std::string& app)
    {
        WmWindow* w = manage(c);
        w->set_app_id(app);
        return w;
    }
};

TEST_F(DockFavoritesTest, a_favorite_has_an_icon_when_it_does_not_run)
{
    set({"a.desktop", "b.desktop"});

    EXPECT_EQ(children_of(dock.get_window()).size(), 3u);
    EXPECT_EQ(dock.get_width(), task_width(2));
    EXPECT_NE(icon_at(0), 0u);
    EXPECT_NE(icon_at(1), 0u);
}

TEST_F(DockFavoritesTest, no_favorites_remove_the_icons)
{
    set({"a.desktop"});
    set({});

    EXPECT_EQ(children_of(dock.get_window()).size(), 1u);
    EXPECT_EQ(dock.get_width(), task_width(0));
}

TEST_F(DockFavoritesTest, a_window_of_a_favorite_takes_its_place)
{
    set({"a.desktop", "b.desktop"});
    FakeClient c;
    WmWindow* w = manage_app(c, "b.desktop");

    add(w);

    EXPECT_EQ(children_of(dock.get_window()).size(), 3u);
    send(icon_at(1), make_event(ButtonRelease));
    EXPECT_TRUE(w->is_focused());
}

TEST_F(DockFavoritesTest, other_windows_come_after_the_favorites)
{
    FakeClient c;
    WmWindow* w = manage_app(c, "x.desktop");
    add(w);
    set({"a.desktop"});

    EXPECT_EQ(children_of(dock.get_window()).size(), 3u);
    send(icon_at(1), make_event(ButtonRelease));
    EXPECT_TRUE(w->is_focused());
}

TEST_F(DockFavoritesTest, a_closed_window_leaves_its_favorite_in_the_dock)
{
    set({"a.desktop"});
    FakeClient c;
    WmWindow* w = manage_app(c, "a.desktop");
    add(w);

    remove(w);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_FALSE(dock.contains(w));
}

TEST_F(DockFavoritesTest, a_second_window_takes_the_place_when_the_first_closes)
{
    set({"a.desktop"});
    FakeClient c1;
    FakeClient c2;
    WmWindow* first = manage_app(c1, "a.desktop");
    WmWindow* second = manage_app(c2, "a.desktop");
    add(first);
    add(second);
    ASSERT_EQ(children_of(dock.get_window()).size(), 3u);

    remove(first);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    send(icon_at(0), make_event(ButtonRelease));
    EXPECT_TRUE(second->is_focused());
}

TEST_F(DockFavoritesTest, the_same_window_is_not_added_twice)
{
    FakeClient c;
    WmWindow* w = manage(c);

    add(w);
    add(w);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
}

TEST_F(DockFavoritesTest, a_click_on_a_favorite_that_does_not_run_launches_it)
{
    set({"a.desktop", "b.desktop"});
    Strings launched;
    dock.on_launch = [&launched](const std::string& id) {
        launched.push_back(id);
    };

    send(icon_at(1), make_event(ButtonRelease));

    EXPECT_EQ(launched, (Strings{"b.desktop"}));
}

TEST_F(DockFavoritesTest, a_click_on_a_favorite_that_runs_does_not_launch_it)
{
    set({"a.desktop"});
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    int launched = 0;
    dock.on_launch = [&launched](const std::string&) { ++launched; };

    send(icon_at(0), make_event(ButtonRelease));

    EXPECT_EQ(launched, 0);
}

TEST_F(DockFavoritesTest, the_icons_change_after_the_click_that_changed_them)
{
    set({"a.desktop", "b.desktop"});
    dock.on_launch = [this](const std::string&) {
        // the clicked icon would be deleted under its own handler
        dock.set_favorites(favorites({"c.desktop"}));
        EXPECT_EQ(children_of(dock.get_window()).size(), 3u); // not yet
    };

    send(icon_at(0), make_event(ButtonRelease));
    pump_events();

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_favorites().size(), 1u);
}

TEST_F(DockFavoritesTest, a_click_without_a_handler_is_ignored)
{
    set({"a.desktop"});

    send(icon_at(0), make_event(ButtonRelease));

    EXPECT_EQ(dock.get_favorites().size(), 1u);
}

TEST_F(DockFavoritesTest, the_menu_of_a_favorite_removes_it)
{
    set({"a.desktop"});
    Strings toggled;
    dock.on_toggle_favorite = [&toggled](const std::string& id) {
        toggled.push_back(id);
    };

    right_click(icon_at(0));

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsViewable);
    EXPECT_EQ(dock.get_menu().toggle.get_text(), "Remove from favorites");
    send(dock.get_menu().toggle, EnterNotify);
    send(dock.get_menu().toggle, ButtonRelease);
    pump_events();
    EXPECT_EQ(toggled, (Strings{"a.desktop"}));
    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockFavoritesTest, the_menu_of_another_window_adds_it)
{
    FakeClient c;
    add(manage_app(c, "x.desktop"));

    right_click(icon_at(0));

    EXPECT_EQ(dock.get_menu().toggle.get_text(), "Add to favorites");
}

TEST_F(DockFavoritesTest, the_menu_is_above_the_icon)
{
    set({"a.desktop"});

    right_click(icon_at(0));

    const XWindowAttributes menu = attributes_of(dock.get_menu());
    // the border is a part of the menu, the icon starts at y_root - y
    EXPECT_EQ(menu.y + menu.height + 2 * menu.border_width, 500 - 10);
}

TEST_F(DockFavoritesTest, a_window_of_unknown_application_has_no_menu)
{
    FakeClient c;
    add(manage(c));

    right_click(icon_at(0));

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockFavoritesTest, the_item_of_the_menu_without_a_handler_only_closes_it)
{
    set({"a.desktop"});
    right_click(icon_at(0));

    send(dock.get_menu().toggle, EnterNotify);
    send(dock.get_menu().toggle, ButtonRelease);
    pump_events();

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockFavoritesTest, the_icons_are_made_when_the_dock_is_realized_later)
{
    tiny::wm::Dock later;
    FakeClient c;
    WmWindow* w = manage_app(c, "a.desktop");
    later.add(w);
    later.set_favorites(favorites({"a.desktop", "b.desktop"}));

    later.realize(root_window(), 0, 0);
    pump_events();

    EXPECT_EQ(children_of(later.get_window()).size(), 3u);
    EXPECT_TRUE(later.contains(w));
}

TEST(DockLifeTest, destructor_destroys_the_icons)
{
    FakeClient c;
    c.map();
    WmWindow w(c.window, root_window(), 0xFFFF, screen_area());
    ::Window dock_window;
    {
        tiny::wm::Dock dock;
        dock.realize(root_window(), 0, 0);
        dock.add(&w);
        dock_window = dock.get_window();
        ASSERT_EQ(children_of(dock_window).size(), 2u);
    }

    const XErrors errors;
    XWindowAttributes attrs;
    EXPECT_EQ(XGetWindowAttributes(tiny::get_display(), dock_window, &attrs),
              0);
}

} // namespace
