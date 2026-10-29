#include "client.h"
#include "helpers.h"

#include <memory>

#include "wm_dock.h"

using WmWindow = wm::Window;

namespace {

int icon_size()
{ return tiny::theme.wm_icon; }

int border()
{ return tiny::theme.wm_dock_border; }

uint32_t widget_bg(tiny::Style::State state)
{ return tiny::theme.widget.get_bg(static_cast<uint8_t>(state)); }

bool has_handler(int type, ::Window window)
{ return tiny::handlers.count({type, window}) == 1; }

// ---- DockButton ----

class DockButtonTest: public ::testing::Test {
  protected:
    void SetUp() override
    {
        button.realize(root_window(), 0, 0);
        button.map_all();
    }

    wm::DockButton button;
};

TEST_F(DockButtonTest, is_a_square_without_border)
{
    EXPECT_EQ(button.get_width(), icon_size());
    EXPECT_EQ(button.get_height(), icon_size());
    EXPECT_EQ(button.get_border(), 0u);
}

TEST_F(DockButtonTest, connects_its_events)
{
    for (int type: {Expose, EnterNotify, LeaveNotify, ButtonRelease}) {
        EXPECT_TRUE(has_handler(type, button.get_window())) << type;
    }
}

TEST_F(DockButtonTest, button_release_emits_click)
{
    Recorder r;
    button.on_click.connect(&r, r.slot());

    send(button, ButtonRelease);

    EXPECT_EQ(r.calls, 1);
    EXPECT_EQ(r.last_owner, &button);
}

TEST_F(DockButtonTest, draws_grid_of_dots)
{
    uint32_t bg = widget_bg(tiny::Style::State::Normal);
    send(button, Expose);

    Image img(button.get_window(), icon_size(), icon_size());
    EXPECT_NE(img.count_not(bg, icon_size()), 0u);
    EXPECT_EQ(img.at(0, 0), bg);
}

TEST(DockButtonScaleTest, dots_are_scaled)
{
    tiny::theme.scale(2);
    {
        wm::DockButton button;
        button.realize(root_window(), 0, 0);
        button.map_all();
        send(button, Expose);

        uint32_t fg = tiny::theme.widget.get_fg(
                static_cast<uint8_t>(tiny::Style::State::Normal));
        Image img(button.get_window(), button.get_width(), button.get_height());
        int origin = 3 * (button.get_width() / 14);

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
    Image hover(button.get_window(), icon_size(), icon_size());

    send(button, LeaveNotify);
    Image normal(button.get_window(), icon_size(), icon_size());

    EXPECT_EQ(hover.at(0, 0),
            widget_bg(tiny::Style::State::Hover));
    EXPECT_EQ(normal.at(0, 0),
            widget_bg(tiny::Style::State::Normal));
}

TEST(DockButtonLifeTest, destructor_disconnects_events)
{
    ::Window window;
    {
        wm::DockButton button;
        button.realize(root_window(), 0, 0);
        window = button.get_window();
    }
    for (int type: {Expose, EnterNotify, LeaveNotify, ButtonRelease}) {
        EXPECT_FALSE(has_handler(type, window)) << type;
    }
}

TEST(DockButtonLifeTest, destructor_of_unrealized_button_is_safe)
{
    wm::DockButton button;
}

// ---- Icon ----

class IconTest: public ::testing::Test {
  protected:
    void SetUp() override
    {
        client.map();
        task = std::make_unique<WmWindow>(client.window, root_window(),
                0xFFFF);
        icon = std::make_unique<wm::Icon>(task.get());
        icon->realize(root_window(), 0, 0);
        icon->map_all();
    }

    Client client;
    std::unique_ptr<WmWindow> task;
    std::unique_ptr<wm::Icon> icon;
};

TEST_F(IconTest, click_focuses_the_task)
{
    ASSERT_FALSE(task->is_focused());
    Recorder r;
    icon->on_click.connect(&r, r.slot());

    send(*icon, ButtonRelease);

    EXPECT_TRUE(task->is_focused());
    EXPECT_EQ(r.calls, 1);
}

TEST_F(IconTest, without_icon_draws_a_circle)
{
    uint32_t bg = widget_bg(tiny::Style::State::Normal);
    send(*icon, Expose);

    Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_NE(img.count_not(bg, icon_size()), 0u);
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2), bg);    // hollow
}

TEST_F(IconTest, draws_the_icon_of_the_task)
{
    icon.reset();
    task.reset();
    client.set_icon_pixmap(icon_size(), false);   // red
    task = std::make_unique<WmWindow>(client.window, root_window(), 0xFFFF);
    icon = std::make_unique<wm::Icon>(task.get());
    icon->realize(root_window(), 0, 0);
    icon->map_all();
    ASSERT_NE(task->get_icon(), 0u);

    send(*icon, Expose);

    Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2), 0xFF0000u);
    EXPECT_EQ(img.at(0, 0), 0xFF0000u);
}

TEST_F(IconTest, focused_task_is_drawn_selected)
{
    send(*icon, Expose);
    Image before(icon->get_window(), icon_size(), icon_size());

    task->set_focus();
    send(*icon, Expose);
    Image after(icon->get_window(), icon_size(), icon_size());

    EXPECT_EQ(before.at(0, 0),
            widget_bg(tiny::Style::State::Normal));
    EXPECT_EQ(after.at(0, 0),
            widget_bg(tiny::Style::State::Selected));
}

// ---- Dock ----

class DockTest: public ::testing::Test {
  protected:
    void SetUp() override
    {
        dock.realize(root_window(), 0, 0);
        dock.set_events();
        dock.map_all();
    }

    void add(WmWindow *w)
    {
        dock.add(w);
        pump_events();      // ConfigureNotify updates the dock size
    }

    void remove(WmWindow *w)
    {
        dock.remove(w);
        pump_events();
    }

    WmWindow *manage(Client &c)
    {
        c.map();
        windows.push_back(std::make_unique<WmWindow>(c.window, root_window(),
                    0xFFFF));
        return windows.back().get();
    }

    int task_width(int tasks) const
    { return (tasks + 1) * (icon_size() + border()) + border(); }

    wm::Dock dock;
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
    Client c;
    add(manage(c));

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(attributes_of(dock).width, task_width(1));
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockTest, icons_are_placed_one_after_another)
{
    Client c1, c2;
    add(manage(c1));
    add(manage(c2));

    EXPECT_NE(child_at(dock.get_window(), border(), border()), 0u);
    EXPECT_NE(child_at(dock.get_window(), 2 * border() + icon_size(), border()),
            0u);
}

TEST_F(DockTest, dock_stays_centered_when_it_grows)
{
    Client c1, c2;
    add(manage(c1));
    add(manage(c2));

    auto root = attributes_of(root_window());
    auto attrs = attributes_of(dock);
    EXPECT_EQ(attrs.x, (root.width / 2 - dock.get_width() / 2));
    EXPECT_EQ(attrs.y, ((root.height - dock.get_height() - border())));
}

TEST_F(DockTest, remove_deletes_the_icon_and_shrinks_the_dock)
{
    Client c1, c2;
    WmWindow *w1 = manage(c1);
    WmWindow *w2 = manage(c2);
    add(w1);
    add(w2);

    remove(w1);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockTest, add_after_remove_gives_the_same_size_as_before)
{
    Client c1, c2;
    WmWindow *w1 = manage(c1);
    WmWindow *w2 = manage(c2);
    add(w1);
    add(w2);
    remove(w1);
    remove(w2);

    EXPECT_EQ(dock.get_width(), task_width(0));
}

TEST_F(DockTest, remove_of_unknown_window_changes_nothing)
{
    Client c;
    WmWindow *w = manage(c);
    add(w);
    Client other;
    WmWindow *unknown = manage(other);

    remove(unknown);

    EXPECT_EQ(children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockTest, redraw_draws_the_icon_of_the_window)
{
    Client c;
    WmWindow *w = manage(c);
    add(w);
    w->set_focus();

    dock.redraw(w, make_event(Expose));

    ::Window icon = child_at(dock.get_window(), border(), border());
    ASSERT_NE(icon, 0u);
    Image img(icon, icon_size(), icon_size());
    EXPECT_EQ(img.at(0, 0),
            widget_bg(tiny::Style::State::Selected));
}

TEST_F(DockTest, redraw_of_unknown_window_is_ignored)
{
    Client c;
    WmWindow *w = manage(c);
    XErrors errors;

    dock.redraw(w, make_event(Expose));

    EXPECT_EQ(errors.errors(), 0);
}

TEST_F(DockTest, clicking_an_icon_focuses_its_window)
{
    Client c;
    WmWindow *w = manage(c);
    add(w);
    ::Window icon = child_at(dock.get_window(), border(), border());
    ASSERT_NE(icon, 0u);

    send(icon, make_event(ButtonRelease));

    EXPECT_TRUE(w->is_focused());
}

TEST(DockLifeTest, destructor_destroys_the_icons)
{
    Client c;
    c.map();
    WmWindow w(c.window, root_window(), 0xFFFF);
    ::Window dock_window;
    {
        wm::Dock dock;
        dock.realize(root_window(), 0, 0);
        dock.add(&w);
        dock_window = dock.get_window();
        ASSERT_EQ(children_of(dock_window).size(), 2u);
    }

    XErrors errors;
    XWindowAttributes attrs;
    EXPECT_EQ(XGetWindowAttributes(tiny::get_display(), dock_window, &attrs),
            0);
}

} // namespace
