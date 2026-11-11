#include "fake_client.h"
#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include <memory>
#include <vector>

#include <X11/Xatom.h>

#include "image/pixmap.h"
#include "wm/dock.h"

using WmWindow = tiny::wm::Window;

namespace {

int icon_size()
{
    return tiny::wm::shell_theme.icon;
}

int border()
{
    return tiny::wm::shell_theme.dock_border;
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

TEST_F(DockButtonTest, draws_the_icon_of_the_launcher_when_it_is_there)
{
    const IconFiles files({"view-app-grid"});
    tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                  {files.dir()});
    button.set_icons(&icons);

    send(button, Expose);

    const Image img(button.get_window(), icon_size(), icon_size());
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    // a square is in the middle, there are no dots
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2),
              tiny::wm::shell_theme.dock.get_fg(normal));
    EXPECT_EQ(img.at(0, 0), tiny::wm::shell_theme.dock.get_bg(normal));
}

TEST(DockButtonScaleTest, dots_are_scaled)
{
    tiny::wm::shell_theme.scale(2);
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
    tiny::wm::shell_theme.scale(1);
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

TEST_F(DockIconTest, without_icon_draws_the_generic_application_icon)
{
    const IconFiles files({"application-x-executable"});
    tiny::wm::SymbolicIcons icons(tiny::get_display(), root_window(),
                                  {files.dir()});
    icon->set_icons(&icons);

    send(*icon, Expose);

    const Image img(icon->get_window(), icon_size(), icon_size());
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    EXPECT_EQ(img.at(icon_size() / 2, icon_size() / 2),
              tiny::wm::lighter(tiny::wm::shell_theme.dock.get_fg(normal)));
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

// The mask of the icon is a clip of the GC: it must not cut off the dot of a
// window that is not a favorite and has only the pixmap of its icon
TEST_F(DockIconTest, the_dot_is_drawn_under_an_icon_with_a_mask)
{
    icon.reset();
    task.reset();
    client.set_icon_pixmap(icon_size(), true);
    task = std::make_unique<WmWindow>(client.window, root_window(), 0xFFFF,
                                      screen_area());
    icon = std::make_unique<tiny::wm::DockIcon>(task.get());
    icon->realize(root_window(), 0, 0);
    icon->map_all();
    ASSERT_NE(task->get_icon_mask(), 0u);

    send(*icon, Expose);

    const int dot = tiny::wm::shell_theme.dock_dot;
    const int left = (icon_size() - dot) / 2;
    const int top = icon_size() + std::max(0, border() - dot - 1);
    const Image img(icon->get_window(), icon->get_width(), icon->get_height());
    const auto state = static_cast<uint8_t>(
            task->is_focused() ? tiny::Style::State::Hover
                               : tiny::Style::State::Normal);
    EXPECT_EQ(img.at(left + dot / 2, top + dot / 2),
              tiny::wm::shell_theme.dock.get_fg(state));
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
              tiny::wm::shell_theme.dock.attention);
    EXPECT_EQ(after.at(icon_size() / 2, icon_size() - 1),
              tiny::wm::shell_theme.dock.attention);
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

// ---- the windows of an application in one icon ----

class DockIconWindowsTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        first = std::make_unique<WmWindow>(client1.window, root_window(),
                                           0xFFFF, screen_area());
        second = std::make_unique<WmWindow>(client2.window, root_window(),
                                            0xFFFF, screen_area());
        client1.map();
        client2.map();
        icon = std::make_unique<tiny::wm::DockIcon>(
                std::vector<WmWindow*>{first.get(), second.get()});
        icon->realize(root_window(), 0, 0);
        icon->map_all();
    }

    // The color of the middle of the dot `index` of `count`
    uint32_t dot_color(int index, int count) const
    {
        const int dot = tiny::wm::shell_theme.dock_dot;
        const int left = (icon_size() - (count * 2 * dot - dot)) / 2;
        const int top = icon_size() + std::max(0, border() - dot - 1);
        const Image img(icon->get_window(), icon->get_width(),
                        icon->get_height());
        return img.at(left + index * 2 * dot + dot / 2, top + dot / 2);
    }

    FakeClient client1;
    FakeClient client2;
    std::unique_ptr<WmWindow> first;
    std::unique_ptr<WmWindow> second;
    std::unique_ptr<tiny::wm::DockIcon> icon;
};

TEST_F(DockIconWindowsTest, is_higher_than_the_icon_by_the_room_for_the_dots)
{
    EXPECT_EQ(icon->get_width(), icon_size());
    EXPECT_EQ(icon->get_height(), icon_size() + border());
}

TEST_F(DockIconWindowsTest, has_a_dot_for_every_window)
{
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    send(*icon, Expose);

    EXPECT_EQ(dot_color(0, 2), tiny::wm::shell_theme.dock.get_fg(normal));
    EXPECT_EQ(dot_color(1, 2), tiny::wm::shell_theme.dock.get_fg(normal));
    EXPECT_EQ(icon->get_windows().size(), 2u);
}

TEST_F(DockIconWindowsTest, the_dot_of_the_focused_window_is_brighter)
{
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    const auto hover = static_cast<uint8_t>(tiny::Style::State::Hover);
    second->set_focus();
    XRaiseWindow(tiny::get_display(), icon->get_window()); // over the client

    send(*icon, Expose);

    EXPECT_EQ(dot_color(0, 2), tiny::wm::shell_theme.dock.get_fg(normal));
    EXPECT_EQ(dot_color(1, 2), tiny::wm::shell_theme.dock.get_fg(hover));
}

TEST_F(DockIconWindowsTest, the_icon_of_a_focused_window_is_selected)
{
    second->set_focus();

    send(*icon, Expose);

    const Image img(icon->get_window(), icon_size(), icon_size());
    EXPECT_EQ(img.at(0, 0), widget_bg(tiny::Style::State::Selected));
}

TEST_F(DockIconWindowsTest, an_application_that_does_not_run_has_no_dots)
{
    tiny::wm::DockIcon idle(nullptr, "a.desktop");
    idle.realize(root_window(), 0, 0);
    idle.map_all();
    const uint32_t bg = widget_bg(tiny::Style::State::Normal);

    send(idle, Expose);

    const Image img(idle.get_window(), idle.get_width(), idle.get_height());
    EXPECT_EQ(img.count_not(bg, idle.get_width()) > 0, true); // the circle
    for (int x = 0; x < idle.get_width(); ++x) {
        EXPECT_EQ(img.at(x, icon_size() + border() / 2), bg);
    }
}

TEST_F(DockIconWindowsTest, a_click_focuses_the_next_window_of_the_application)
{
    send(*icon, ButtonRelease);
    EXPECT_TRUE(first->is_focused());

    send(*icon, ButtonRelease);
    EXPECT_TRUE(second->is_focused());

    send(*icon, ButtonRelease);
    EXPECT_TRUE(first->is_focused());
}

TEST_F(DockIconWindowsTest, knows_its_windows)
{
    EXPECT_TRUE(icon->contains(first.get()));
    EXPECT_TRUE(icon->contains(second.get()));
    EXPECT_FALSE(icon->contains(nullptr));
    EXPECT_EQ(icon->get_task(), first.get());
}

TEST_F(DockIconWindowsTest, the_dots_do_not_go_over_the_width_of_the_icon)
{
    std::vector<std::unique_ptr<FakeClient>> clients;
    std::vector<std::unique_ptr<WmWindow>> owned;
    std::vector<WmWindow*> many;
    for (int i = 0; i < 20; ++i) {
        clients.push_back(std::make_unique<FakeClient>());
        clients.back()->map();
        owned.push_back(std::make_unique<WmWindow>(
                clients.back()->window, root_window(), 0xFFFF, screen_area()));
        many.push_back(owned.back().get());
    }
    tiny::wm::DockIcon crowded(many);
    crowded.realize(root_window(), 0, 0);
    crowded.map_all();

    const XErrors errors;
    send(crowded, Expose);

    EXPECT_EQ(errors.errors(), 0);
}

// ---- drag of a favorite ----

class DockIconDragTest : public DockIconFavoriteTest {
  protected:
    void SetUp() override
    {
        DockIconFavoriteTest::SetUp();
        icon->set_draggable(true);
        icon->on_drag = [this](const tiny::wm::DockIcon&, int x) {
            dragged.push_back(x);
        };
        icon->on_drop = [this](const tiny::wm::DockIcon&, int x) {
            dropped.push_back(x);
        };
        icon->on_click.connect(click.slot());
    }

    void press(int x_root)
    {
        XEvent e = make_event(ButtonPress);
        e.xbutton.button = Button1;
        e.xbutton.x_root = x_root;
        send(icon->get_window(), e);
    }

    void move(int x_root)
    {
        XEvent e = make_event(MotionNotify);
        e.xmotion.x_root = x_root;
        send(icon->get_window(), e);
    }

    void release(int x_root)
    {
        XEvent e = make_event(ButtonRelease);
        e.xbutton.button = Button1;
        e.xbutton.x_root = x_root;
        send(icon->get_window(), e);
    }

    Recorder click;
    std::vector<int> dragged;
    std::vector<int> dropped;
};

TEST_F(DockIconDragTest, a_small_move_is_not_a_drag)
{
    press(100);
    move(100 + tiny::wm::shell_theme.dock_drag - 1);

    EXPECT_FALSE(icon->is_dragging());
    EXPECT_TRUE(dragged.empty());
}

TEST_F(DockIconDragTest, a_longer_move_starts_the_drag_and_tells_every_move)
{
    press(100);
    move(100 + tiny::wm::shell_theme.dock_drag);
    move(140);

    EXPECT_TRUE(icon->is_dragging());
    EXPECT_EQ(dragged,
              (std::vector<int>{100 + tiny::wm::shell_theme.dock_drag, 140}));
}

TEST_F(DockIconDragTest, a_move_to_the_left_drags_too)
{
    press(100);
    move(60);

    EXPECT_TRUE(icon->is_dragging());
}

TEST_F(DockIconDragTest, the_release_drops_the_icon_and_is_not_a_click)
{
    press(100);
    move(150);
    release(160);

    EXPECT_EQ(dropped, (std::vector<int>{160}));
    EXPECT_FALSE(icon->is_dragging());
    EXPECT_EQ(click.calls, 0);
}

TEST_F(DockIconDragTest, escape_cancels_the_drag_and_the_release_does_nothing)
{
    int cancelled = 0;
    icon->on_cancel = [&cancelled](const tiny::wm::DockIcon&) { ++cancelled; };
    press(100);
    move(150);
    ASSERT_TRUE(icon->is_dragging());

    send(icon->get_window(), key_event(icon->get_window(), XK_Escape));

    EXPECT_FALSE(icon->is_dragging());
    EXPECT_TRUE(icon->is_cancelled());
    EXPECT_EQ(cancelled, 1);
    move(200); // the button is still down: no more drag
    EXPECT_EQ(dragged.size(), 1u);
    release(210);
    EXPECT_TRUE(dropped.empty());
    EXPECT_EQ(click.calls, 0);
    EXPECT_FALSE(icon->is_cancelled());
}

TEST_F(DockIconDragTest, a_new_press_after_a_cancel_drags_again)
{
    press(100);
    move(150);
    send(icon->get_window(), key_event(icon->get_window(), XK_Escape));
    release(150);

    press(100);
    move(150);

    EXPECT_TRUE(icon->is_dragging());
}

TEST_F(DockIconDragTest, other_keys_and_keys_without_a_drag_do_nothing)
{
    send(icon->get_window(), key_event(icon->get_window(), XK_Escape));
    EXPECT_FALSE(icon->is_cancelled());

    press(100);
    move(150);
    send(icon->get_window(), key_event(icon->get_window(), XK_a));

    EXPECT_TRUE(icon->is_dragging());
}

TEST_F(DockIconDragTest, the_release_without_a_drag_is_a_click)
{
    press(100);
    move(101);
    release(101);

    EXPECT_TRUE(dropped.empty());
    EXPECT_EQ(click.calls, 1);
}

TEST_F(DockIconDragTest, a_move_without_a_press_does_nothing)
{
    move(500);

    EXPECT_FALSE(icon->is_dragging());
    EXPECT_TRUE(dragged.empty());
}

TEST_F(DockIconDragTest, an_icon_that_is_not_draggable_stays)
{
    icon->set_draggable(false);

    press(100);
    move(300);

    EXPECT_FALSE(icon->is_dragging());
    EXPECT_TRUE(dragged.empty());
}

TEST_F(DockIconDragTest, a_drag_without_handlers_is_ignored)
{
    icon->on_drag = nullptr;
    icon->on_drop = nullptr;

    press(100);
    move(200);
    release(200);

    EXPECT_FALSE(icon->is_dragging());
    EXPECT_EQ(click.calls, 0);
}

TEST_F(DockIconDragTest, the_icon_is_drawn_smaller_while_it_is_dragged)
{
    if (pixmap == None) {
        GTEST_SKIP() << "no ARGB32 format";
    }
    const uint32_t green = 0x00FF00;
    send(*icon, Expose);
    const Image whole(icon->get_window(), icon_size(), icon_size());
    ASSERT_EQ(whole.at(2, 2), green);

    press(100);
    move(200);
    const Image small(icon->get_window(), icon_size(), icon_size());

    EXPECT_NE(small.at(2, 2), green);
    EXPECT_EQ(small.at(icon_size() / 2, icon_size() / 2), green);

    release(200);
    const Image back(icon->get_window(), icon_size(), icon_size());
    EXPECT_EQ(back.at(2, 2), green);
}

TEST_F(DockIconDragTest, an_icon_without_a_picture_is_smaller_too)
{
    tiny::wm::DockIcon bare(nullptr, "b.desktop");
    bare.set_draggable(true);
    bare.realize(root_window(), 0, 0);
    bare.map_all();
    const uint32_t bg = widget_bg(tiny::Style::State::Normal);
    XEvent e = make_event(ButtonPress);
    e.xbutton.button = Button1;
    e.xbutton.x_root = 10;
    send(bare.get_window(), e);
    e = make_event(MotionNotify);
    e.xmotion.x_root = 100;
    send(bare.get_window(), e);

    const Image img(bare.get_window(), icon_size(), icon_size());

    EXPECT_EQ(img.at(1, icon_size() / 2), bg); // the circle is smaller
    EXPECT_NE(img.count_not(bg, icon_size()), 0u);
}

TEST(DockIconLifeTest, destructor_disconnects_the_drag_events)
{
    ::Window window;
    {
        tiny::wm::DockIcon icon(nullptr, "a.desktop");
        icon.realize(root_window(), 0, 0);
        window = icon.get_window();
        EXPECT_TRUE(has_handler(ButtonPress, window));
        EXPECT_TRUE(has_handler(MotionNotify, window));
    }
    EXPECT_FALSE(has_handler(ButtonPress, window));
    EXPECT_FALSE(has_handler(MotionNotify, window));
}

// ---- group_dock ----

using Strings = std::vector<std::string>;

TEST(GroupDockTest, nothing_is_empty)
{
    EXPECT_TRUE(tiny::wm::group_dock({}, {}).empty());
}

TEST(GroupDockTest, windows_of_one_application_are_one_icon)
{
    const auto groups = tiny::wm::group_dock({}, {"a", "b", "a"});

    ASSERT_EQ(groups.size(), 2u);
    EXPECT_EQ(groups[0].favorite, -1);
    EXPECT_EQ(groups[0].key, "a");
    EXPECT_EQ(groups[0].tasks, (std::vector<size_t>{0, 2}));
    EXPECT_EQ(groups[1].tasks, (std::vector<size_t>{1}));
}

TEST(GroupDockTest, every_window_of_unknown_application_is_an_icon)
{
    const auto groups = tiny::wm::group_dock({}, {"", "", "b"});

    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[0].tasks, (std::vector<size_t>{0}));
    EXPECT_EQ(groups[1].tasks, (std::vector<size_t>{1}));
    EXPECT_EQ(groups[2].tasks, (std::vector<size_t>{2}));
}

TEST(GroupDockTest, favorites_that_do_not_run_come_first_and_have_no_window)
{
    const auto groups = tiny::wm::group_dock({"a", "b"}, {"c"});

    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[0].favorite, 0);
    EXPECT_TRUE(groups[0].tasks.empty());
    EXPECT_EQ(groups[1].favorite, 1);
    EXPECT_TRUE(groups[1].tasks.empty());
    EXPECT_EQ(groups[2].favorite, -1);
    EXPECT_EQ(groups[2].tasks, (std::vector<size_t>{0}));
}

TEST(GroupDockTest, all_windows_of_a_favorite_are_at_its_place)
{
    const auto groups = tiny::wm::group_dock({"a", "b"}, {"c", "b", "b"});

    ASSERT_EQ(groups.size(), 3u);
    EXPECT_TRUE(groups[0].tasks.empty());
    EXPECT_EQ(groups[1].favorite, 1);
    EXPECT_EQ(groups[1].tasks, (std::vector<size_t>{1, 2}));
    EXPECT_EQ(groups[2].favorite, -1);
    EXPECT_EQ(groups[2].tasks, (std::vector<size_t>{0}));
}

TEST(GroupDockTest, a_window_never_joins_a_group_of_another_application)
{
    const auto groups = tiny::wm::group_dock({"a"}, {""});

    ASSERT_EQ(groups.size(), 2u);
    EXPECT_TRUE(groups[0].tasks.empty());
    EXPECT_EQ(groups[1].tasks, (std::vector<size_t>{0}));
}

// ---- helpers of the layout ----

TEST(DropPositionTest, counts_the_icons_left_of_the_pointer)
{
    const std::vector<int> centers = {10, 30, 50};

    EXPECT_EQ(tiny::wm::drop_position(centers, 0), 0u);
    EXPECT_EQ(tiny::wm::drop_position(centers, 31), 2u);
    EXPECT_EQ(tiny::wm::drop_position(centers, 500), 3u);
    EXPECT_EQ(tiny::wm::drop_position({}, 5), 0u);
}

TEST(MenuTitleTest, a_short_title_stays)
{
    EXPECT_EQ(tiny::wm::menu_title("Terminal", 40), "Terminal");
}

TEST(MenuTitleTest, a_long_title_ends_with_an_ellipsis)
{
    EXPECT_EQ(tiny::wm::menu_title("abcdef", 3), "abc\xE2\x80\xA6");
    EXPECT_EQ(tiny::wm::menu_title("abc", 3), "abc");
}

TEST(MenuTitleTest, a_character_is_never_cut_in_the_middle)
{
    // each of the letters is two bytes
    EXPECT_EQ(tiny::wm::menu_title("\xC4\x9B\xC5\xA1\xC4\x8D\xC5\x99", 2),
              "\xC4\x9B\xC5\xA1\xE2\x80\xA6");
}

TEST(MenuTitleTest, a_window_without_a_title_has_a_name)
{
    EXPECT_EQ(tiny::wm::menu_title("", 40), "Untitled");
}

TEST(PlaceMenuTest, opens_above_the_anchor_in_the_middle)
{
    const auto place =
            tiny::wm::place_menu(300, 500, 100, 60, true, true, 1000, 800);

    EXPECT_EQ(place.x, 250);
    EXPECT_EQ(place.y, 440);
}

TEST(PlaceMenuTest, opens_below_the_anchor_from_it)
{
    const auto place =
            tiny::wm::place_menu(300, 500, 100, 60, false, false, 1000, 800);

    EXPECT_EQ(place.x, 300);
    EXPECT_EQ(place.y, 500);
}

TEST(PlaceMenuTest, stays_in_the_screen)
{
    const auto right =
            tiny::wm::place_menu(990, 790, 100, 60, false, false, 1000, 800);
    const auto left =
            tiny::wm::place_menu(-20, 10, 100, 60, true, true, 1000, 800);

    EXPECT_EQ(right.x, 900);
    EXPECT_EQ(right.y, 740);
    EXPECT_EQ(left.x, 0);
    EXPECT_EQ(left.y, 0);
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
    // the dots of the windows are under the icons
    EXPECT_EQ(dock.get_height(), icon_size() + border() * 2);
    EXPECT_EQ(dock.get_style().padding_width, 5u);
}

TEST_F(DockTest, menu_button_is_in_the_dock)
{
    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 1u);
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

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
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

namespace {

std::vector<long> icon_geometry_of(const FakeClient& c)
{
    XSync(dsp(), False);
    Atom type;
    int format;
    unsigned long count;
    unsigned long left;
    unsigned char* data = nullptr;
    std::vector<long> values;
    if (XGetWindowProperty(dsp(), c.window, dsp()._NET_WM_ICON_GEOMETRY, 0, 4,
                           False, XA_CARDINAL, &type, &format, &count, &left,
                           &data) == Success &&
        data) {
        const auto* first = reinterpret_cast<const long*>(data);
        values.assign(first, first + count);
        XFree(data);
    }
    return values;
}

} // namespace

TEST_F(DockTest, a_window_is_told_where_its_icon_is)
{
    FakeClient c1;
    FakeClient c2;
    add(manage(c1));
    add(manage(c2));
    pump_events();

    // the root coordinates of the icons, as the server has them
    for (const FakeClient* c : {&c1, &c2}) {
        const auto geometry = icon_geometry_of(*c);
        ASSERT_EQ(geometry.size(), 4u);
        bool found = false;
        for (const ::Window kid : mapped_children_of(dock.get_window())) {
            int x = 0;
            int y = 0;
            ::Window unused;
            XTranslateCoordinates(dsp(), kid, root_window(), 0, 0, &x, &y,
                                  &unused);
            found = found ||
                    (x == geometry[0] && y == geometry[1] &&
                     geometry[2] == icon_size() && geometry[3] == icon_size());
        }
        EXPECT_TRUE(found);
    }
    EXPECT_NE(icon_geometry_of(c1)[0], icon_geometry_of(c2)[0]);
}

TEST_F(DockTest, a_window_that_leaves_the_dock_has_no_icon_geometry)
{
    FakeClient c;
    WmWindow* window = manage(c);
    add(window);
    ASSERT_FALSE(icon_geometry_of(c).empty());

    remove(window);

    EXPECT_TRUE(icon_geometry_of(c).empty());
}

TEST_F(DockTest, the_icons_follow_the_dock_to_another_monitor)
{
    FakeClient c;
    add(manage(c));
    const long before = icon_geometry_of(c)[0];

    dock.set_screen({100, 50, 800, 600});

    const auto geometry = icon_geometry_of(c);
    EXPECT_NE(geometry[0], before);
    EXPECT_EQ(geometry[1], dock.area().y + border());
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

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
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

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
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
    const ShellFonts shell_fonts;

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

    // The window of the icon after the line that follows `favorites` icons
    ::Window icon_after_line(int favorites) const
    {
        return child_at(dock.get_window(),
                        border() + favorites * (icon_size() + border()) +
                                border() + tiny::wm::shell_theme.dock_line,
                        border());
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

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 3u);
    EXPECT_EQ(dock.get_width(), task_width(2));
    EXPECT_NE(icon_at(0), 0u);
    EXPECT_NE(icon_at(1), 0u);
}

TEST_F(DockFavoritesTest, no_favorites_remove_the_icons)
{
    set({"a.desktop"});
    set({});

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 1u);
    EXPECT_EQ(dock.get_width(), task_width(0));
}

TEST_F(DockFavoritesTest, a_window_of_a_favorite_takes_its_place)
{
    set({"a.desktop", "b.desktop"});
    FakeClient c;
    WmWindow* w = manage_app(c, "b.desktop");

    add(w);

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 3u);
    send(icon_at(1), make_event(ButtonRelease));
    EXPECT_TRUE(w->is_focused());
}

TEST_F(DockFavoritesTest, other_windows_come_after_the_favorites)
{
    FakeClient c;
    WmWindow* w = manage_app(c, "x.desktop");
    add(w);
    set({"a.desktop"});

    // the menu button, two icons and the line between them
    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 4u);
    send(icon_after_line(1), make_event(ButtonRelease));
    EXPECT_TRUE(w->is_focused());
}

TEST_F(DockFavoritesTest, a_closed_window_leaves_its_favorite_in_the_dock)
{
    set({"a.desktop"});
    FakeClient c;
    WmWindow* w = manage_app(c, "a.desktop");
    add(w);

    remove(w);

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
    EXPECT_FALSE(dock.contains(w));
}

TEST_F(DockFavoritesTest, the_second_window_stays_when_the_first_closes)
{
    set({"a.desktop"});
    FakeClient c1;
    FakeClient c2;
    WmWindow* first = manage_app(c1, "a.desktop");
    WmWindow* second = manage_app(c2, "a.desktop");
    add(first);
    add(second);
    ASSERT_EQ(mapped_children_of(dock.get_window()).size(), 2u); // one icon

    remove(first);

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
    send(icon_at(0), make_event(ButtonRelease));
    EXPECT_TRUE(second->is_focused());
}

TEST_F(DockFavoritesTest, the_same_window_is_not_added_twice)
{
    FakeClient c;
    WmWindow* w = manage(c);

    add(w);
    add(w);

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
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
        EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 3u); // not yet
    };

    send(icon_at(0), make_event(ButtonRelease));
    pump_events();

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_favorites().size(), 1u);
}

TEST_F(DockFavoritesTest, a_click_without_a_handler_is_ignored)
{
    set({"a.desktop"});

    send(icon_at(0), make_event(ButtonRelease));

    EXPECT_EQ(dock.get_favorites().size(), 1u);
}

// The texts of the items of the menu
Strings menu_texts(tiny::wm::DockMenu& menu)
{
    Strings texts;
    for (size_t i = 0; i < menu.count(); ++i) {
        texts.push_back(menu.item(i).get_text());
    }
    return texts;
}

// A click on an item of the menu (the pointer enters it first)
void click_item(tiny::wm::DockMenu& menu, size_t index)
{
    send(menu.item(index), EnterNotify);
    send(menu.item(index), ButtonRelease);
    pump_events();
}

TEST_F(DockFavoritesTest,
       the_menu_starts_with_new_window_and_ends_with_the_favorite)
{
    set({"a.desktop"});

    right_click(icon_at(0));

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsViewable);
    EXPECT_EQ(menu_texts(dock.get_menu()),
              (Strings{"New Window", "Remove from Favorites"}));
}

TEST_F(DockFavoritesTest,
       new_window_starts_the_application_again_even_when_it_runs)
{
    set({"a.desktop"});
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    Strings launched;
    dock.on_launch = [&launched](const std::string& id) {
        launched.push_back(id);
    };

    right_click(icon_at(0));
    click_item(dock.get_menu(), 0);

    EXPECT_EQ(launched, (Strings{"a.desktop"}));
    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockFavoritesTest, the_actions_of_the_application_are_between_the_items)
{
    set({"a.desktop"});
    dock.list_actions = [](const std::string& id) {
        return id == "a.desktop" ? Strings{"Private", "Compose"} : Strings{};
    };
    std::vector<std::pair<std::string, size_t>> chosen;
    dock.on_action = [&chosen](const std::string& id, size_t index) {
        chosen.emplace_back(id, index);
    };

    right_click(icon_at(0));
    ASSERT_EQ(menu_texts(dock.get_menu()),
              (Strings{"New Window", "Private", "Compose",
                       "Remove from Favorites"}));
    click_item(dock.get_menu(), 2);

    ASSERT_EQ(chosen.size(), 1u);
    EXPECT_EQ(chosen[0].first, "a.desktop");
    EXPECT_EQ(chosen[0].second, 1u); // the second action
}

TEST_F(DockFavoritesTest, the_menu_is_made_again_for_every_icon)
{
    set({"a.desktop", "b.desktop"});
    dock.list_actions = [](const std::string& id) {
        return id == "a.desktop" ? Strings{"Private"} : Strings{};
    };

    right_click(icon_at(0));
    EXPECT_EQ(dock.get_menu().count(), 3u);
    dock.get_menu().unmap();
    right_click(icon_at(1));

    EXPECT_EQ(menu_texts(dock.get_menu()),
              (Strings{"New Window", "Remove from Favorites"}));
    EXPECT_EQ(attributes_of(dock.get_menu()).height,
              2 * tiny::wm::shell_theme.popup_item_height);
}

TEST_F(DockFavoritesTest, an_item_without_a_handler_only_closes_the_menu)
{
    set({"a.desktop"});
    dock.list_actions = [](const std::string&) { return Strings{"Private"}; };

    for (const size_t index : {size_t{0}, size_t{1}, size_t{2}}) {
        right_click(icon_at(0));
        click_item(dock.get_menu(), index);
        EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped)
                << index;
    }
}

TEST_F(DockFavoritesTest, the_menu_of_a_favorite_removes_it)
{
    set({"a.desktop"});
    Strings toggled;
    dock.on_toggle_favorite = [&toggled](const std::string& id) {
        toggled.push_back(id);
    };

    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    EXPECT_EQ(toggled, (Strings{"a.desktop"}));
    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockFavoritesTest, the_menu_of_another_window_adds_it)
{
    FakeClient c;
    add(manage_app(c, "x.desktop"));

    right_click(icon_at(0));

    EXPECT_EQ(menu_texts(dock.get_menu()),
              (Strings{"New Window", "All Windows \xE2\x80\xBA",
                       "Add to Favorites", "Close Window"}));
}

TEST_F(DockFavoritesTest, the_menu_is_above_the_icon)
{
    set({"a.desktop"});
    dock.list_actions = [](const std::string&) { return Strings{"Private"}; };

    right_click(icon_at(0));

    const XWindowAttributes menu = attributes_of(dock.get_menu());
    // the border is a part of the menu, the icon starts at y_root - y
    EXPECT_EQ(menu.y + menu.height + 2 * menu.border_width, 500 - 10);
    EXPECT_EQ(menu.height, 3 * tiny::wm::shell_theme.popup_item_height);
}

TEST_F(DockFavoritesTest, a_window_of_unknown_application_has_no_launcher_items)
{
    FakeClient c;
    add(manage(c));

    right_click(icon_at(0));

    EXPECT_EQ(menu_texts(dock.get_menu()),
              (Strings{"All Windows \xE2\x80\xBA", "Close Window"}));
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

    EXPECT_EQ(children_of(later.get_window()).size(), 4u); // the mark too
    EXPECT_TRUE(later.contains(w));
}

// ---- groups, the line, the menu with the windows, the drag ----

class DockGroupsTest : public DockFavoritesTest {
  protected:
    int width_of_menu(tiny::wm::DockMenu& menu)
    {
        int text = 0;
        for (size_t i = 0; i < menu.count(); ++i) {
            text = std::max(text, menu.item(i).text_width());
        }
        return text + 2 * tiny::wm::shell_theme.menu_padding;
    }

    // The root x of the middle of the favorite icon `index`
    int center_of(int index)
    {
        return attributes_of(dock).x + border() +
               index * (icon_size() + border()) + icon_size() / 2;
    }

    void press(::Window icon, int x_root)
    {
        XEvent e = make_event(ButtonPress);
        e.xbutton.button = Button1;
        e.xbutton.x_root = x_root;
        send(icon, e);
    }

    void move(::Window icon, int x_root)
    {
        XEvent e = make_event(MotionNotify);
        e.xmotion.x_root = x_root;
        send(icon, e);
    }

    void release(::Window icon, int x_root)
    {
        XEvent e = make_event(ButtonRelease);
        e.xbutton.button = Button1;
        e.xbutton.x_root = x_root;
        send(icon, e);
        pump_events();
    }
};

TEST_F(DockGroupsTest, windows_of_one_application_have_one_icon)
{
    FakeClient c1;
    FakeClient c2;
    FakeClient c3;
    add(manage_app(c1, "x.desktop"));
    add(manage_app(c2, "x.desktop"));
    add(manage_app(c3, "y.desktop"));

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(),
              3u); // x, y, the button
}

TEST_F(DockGroupsTest, windows_of_one_unknown_class_have_one_icon)
{
    FakeClient c1;
    FakeClient c2;
    c1.set_class("one", "Same");
    c2.set_class("two", "Same");
    add(manage(c1));
    add(manage(c2));

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
}

TEST_F(DockGroupsTest, windows_without_application_and_class_have_an_icon_each)
{
    FakeClient c1;
    FakeClient c2;
    add(manage(c1));
    add(manage(c2));

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 3u);
}

TEST_F(DockGroupsTest, a_line_divides_the_favorites_from_the_other_applications)
{
    set({"a.desktop"});
    FakeClient c;
    add(manage_app(c, "x.desktop"));

    const int inset = icon_size() / 6;
    const ::Window line = child_at(
            dock.get_window(), 2 * border() + icon_size(), border() + inset);
    ASSERT_NE(line, 0u);
    const XWindowAttributes attrs = attributes_of(line);
    EXPECT_EQ(attrs.width, tiny::wm::shell_theme.dock_line);
    // it does not touch the edges of the icons
    EXPECT_EQ(attrs.height, icon_size() - 2 * inset);
    EXPECT_LT(attrs.height, icon_size());
    EXPECT_EQ(dock.get_width(),
              task_width(2) + border() + tiny::wm::shell_theme.dock_line);
}

TEST_F(DockGroupsTest, the_line_has_the_color_of_the_theme)
{
    set({"a.desktop"});
    FakeClient c;
    add(manage_app(c, "x.desktop"));

    const ::Window line =
            child_at(dock.get_window(), 2 * border() + icon_size(),
                     border() + icon_size() / 6);
    const Image img(line, tiny::wm::shell_theme.dock_line, 1);

    EXPECT_EQ(img.at(0, 0),
              tiny::wm::shell_theme.dock_separator.get_bg(
                      static_cast<uint8_t>(tiny::Style::State::Normal)));
}

TEST_F(DockGroupsTest, there_is_no_line_without_the_other_applications)
{
    set({"a.desktop", "b.desktop"});

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 3u);
    EXPECT_EQ(dock.get_width(), task_width(2));
}

TEST_F(DockGroupsTest, there_is_no_line_without_favorites)
{
    FakeClient c;
    add(manage_app(c, "x.desktop"));

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
}

TEST_F(DockGroupsTest, the_line_goes_with_the_last_other_application)
{
    set({"a.desktop"});
    FakeClient c;
    WmWindow* w = manage_app(c, "x.desktop");
    add(w);
    ASSERT_EQ(mapped_children_of(dock.get_window()).size(), 4u);

    remove(w);

    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 2u);
    EXPECT_EQ(dock.get_width(), task_width(1));
}

TEST_F(DockGroupsTest, a_window_of_a_group_is_redrawn_in_the_icon_of_the_group)
{
    FakeClient c1;
    FakeClient c2;
    add(manage_app(c1, "x.desktop"));
    WmWindow* second = manage_app(c2, "x.desktop");
    add(second);
    second->set_focus();

    dock.redraw(second, make_event(Expose));

    const Image img(icon_at(0), icon_size(), icon_size());
    EXPECT_EQ(img.at(0, 0), widget_bg(tiny::Style::State::Selected));
}

TEST_F(DockGroupsTest, the_menu_is_as_wide_as_its_longest_text_and_a_margin)
{
    set({"a.desktop"});
    dock.list_actions = [](const std::string&) {
        return Strings{"A very long name of an action of the application"};
    };

    right_click(icon_at(0));

    EXPECT_EQ(attributes_of(dock.get_menu()).width,
              width_of_menu(dock.get_menu()));
    EXPECT_GT(attributes_of(dock.get_menu()).width,
              dock.get_menu().item(0).text_width() +
                      2 * tiny::wm::shell_theme.menu_padding);
}

TEST_F(DockGroupsTest, a_short_menu_is_narrow)
{
    set({"a.desktop"});

    right_click(icon_at(0));
    const int narrow = attributes_of(dock.get_menu()).width;
    dock.get_menu().unmap();
    dock.list_actions = [](const std::string&) {
        return Strings{"A very long name of an action of the application"};
    };
    right_click(icon_at(0));

    EXPECT_EQ(narrow, width_of_menu(dock.get_menu()) -
                              (dock.get_menu().content_width() - narrow));
    EXPECT_LT(narrow, attributes_of(dock.get_menu()).width);
}

TEST_F(DockGroupsTest, the_items_have_room_around_the_text)
{
    set({"a.desktop"});

    right_click(icon_at(0));

    EXPECT_GE(tiny::wm::shell_theme.popup_item_height,
              tiny::wm::shell_theme.panel);
    const XWindowAttributes item = attributes_of(dock.get_menu().item(1));
    EXPECT_EQ(item.height, tiny::wm::shell_theme.popup_item_height);
    EXPECT_EQ(item.y, tiny::wm::shell_theme.popup_item_height);
    EXPECT_EQ(item.width, dock.get_menu().content_width());
}

TEST_F(DockGroupsTest,
       the_menu_of_a_running_application_has_the_windows_and_close)
{
    set({"a.desktop"});
    FakeClient c1;
    FakeClient c2;
    add(manage_app(c1, "a.desktop"));
    add(manage_app(c2, "a.desktop"));

    right_click(icon_at(0));

    EXPECT_EQ(menu_texts(dock.get_menu()),
              (Strings{"New Window", "All Windows \xE2\x80\xBA",
                       "Remove from Favorites", "Close All Windows"}));
}

TEST_F(DockGroupsTest, all_windows_opens_a_submenu_with_their_titles)
{
    set({"a.desktop"});
    FakeClient c1;
    FakeClient c2;
    c1.set_title("First window");
    c2.set_title(std::string(60, 'x'));
    add(manage_app(c1, "a.desktop"));
    add(manage_app(c2, "a.desktop"));

    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    EXPECT_EQ(attributes_of(dock.get_windows_menu()).map_state, IsViewable);
    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsViewable);
    EXPECT_EQ(menu_texts(dock.get_windows_menu()),
              (Strings{"First window", std::string(40, 'x') + "\xE2\x80\xA6"}));
}

TEST_F(DockGroupsTest, the_submenu_is_next_to_the_item)
{
    set({"a.desktop"});
    FakeClient c;
    add(manage_app(c, "a.desktop"));

    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    const XWindowAttributes menu = attributes_of(dock.get_menu());
    const XWindowAttributes sub = attributes_of(dock.get_windows_menu());
    EXPECT_EQ(sub.x, menu.x + menu.width + 2 * menu.border_width);
    EXPECT_EQ(sub.y, menu.y + tiny::wm::shell_theme.popup_item_height);
}

TEST_F(DockGroupsTest, the_submenu_opens_to_the_left_at_the_edge_of_the_screen)
{
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    const int width = attributes_of(root_window()).width;

    dock.popup_menu("a.desktop", width - 10, 10);
    pump_events();
    click_item(dock.get_menu(), 1);

    const XWindowAttributes menu = attributes_of(dock.get_menu());
    const XWindowAttributes sub = attributes_of(dock.get_windows_menu());
    EXPECT_LT(sub.x, menu.x);
    EXPECT_LE(sub.x + sub.width + 2 * sub.border_width, menu.x);
}

TEST_F(DockGroupsTest, a_window_of_the_submenu_gets_the_focus)
{
    FakeClient c1;
    FakeClient c2;
    add(manage_app(c1, "a.desktop"));
    WmWindow* second = manage_app(c2, "a.desktop");
    add(second);

    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);
    click_item(dock.get_windows_menu(), 1);

    EXPECT_TRUE(second->is_focused());
    EXPECT_EQ(attributes_of(dock.get_windows_menu()).map_state, IsUnmapped);
    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, closing_the_submenu_closes_the_menu)
{
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    dock.get_windows_menu().unmap();

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, closing_the_menu_closes_the_submenu)
{
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    dock.get_menu().unmap();

    EXPECT_EQ(attributes_of(dock.get_windows_menu()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, the_menu_tells_when_it_is_closed)
{
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    int closed = 0;
    dock.on_menu_closed = [&closed] { ++closed; };
    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    dock.get_windows_menu().unmap();

    EXPECT_EQ(closed, 1);
}

TEST_F(DockGroupsTest, a_closed_window_closes_the_menu)
{
    FakeClient c1;
    FakeClient c2;
    add(manage_app(c1, "a.desktop"));
    WmWindow* second = manage_app(c2, "a.desktop");
    add(second);
    right_click(icon_at(0));
    ASSERT_EQ(attributes_of(dock.get_menu()).map_state, IsViewable);

    remove(second);

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, close_window_asks_the_window_to_close)
{
    FakeClient c;
    c.set_protocols({dsp().WM_DELETE_WINDOW});
    add(manage_app(c, "a.desktop"));

    right_click(icon_at(0));
    ASSERT_EQ(menu_texts(dock.get_menu()).back(), "Close Window");
    click_item(dock.get_menu(), dock.get_menu().count() - 1);

    XEvent e;
    ASSERT_TRUE(c.message(e));
    EXPECT_EQ(e.xclient.data.l[0], static_cast<long>(dsp().WM_DELETE_WINDOW));
    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, close_all_windows_asks_every_window_to_close)
{
    FakeClient c1;
    FakeClient c2;
    c1.set_protocols({dsp().WM_DELETE_WINDOW});
    c2.set_protocols({dsp().WM_DELETE_WINDOW});
    add(manage_app(c1, "a.desktop"));
    add(manage_app(c2, "a.desktop"));

    right_click(icon_at(0));
    click_item(dock.get_menu(), dock.get_menu().count() - 1);

    XEvent e;
    EXPECT_TRUE(c1.message(e));
    EXPECT_TRUE(c2.message(e));
}

TEST_F(DockGroupsTest,
       the_menu_of_an_application_that_does_not_run_cannot_close)
{
    set({"a.desktop"});

    right_click(icon_at(0));

    for (const std::string& text : menu_texts(dock.get_menu())) {
        EXPECT_EQ(text.find("Close"), std::string::npos) << text;
        EXPECT_EQ(text.find("All Windows"), std::string::npos) << text;
    }
}

TEST_F(DockGroupsTest, the_menu_can_be_opened_at_the_pointer)
{
    FakeClient c;
    add(manage_app(c, "x.desktop"));

    dock.popup_menu("x.desktop", 100, 200);
    pump_events();

    const XWindowAttributes menu = attributes_of(dock.get_menu());
    EXPECT_EQ(menu.map_state, IsViewable);
    EXPECT_EQ(menu.x, 100);
    EXPECT_EQ(menu.y, 200);
    EXPECT_EQ(menu_texts(dock.get_menu()),
              (Strings{"New Window", "All Windows \xE2\x80\xBA",
                       "Add to Favorites", "Close Window"}));
}

TEST_F(DockGroupsTest,
       the_menu_of_an_application_that_does_not_run_at_the_pointer)
{
    set({"a.desktop"});
    Strings launched;
    dock.on_launch = [&launched](const std::string& id) {
        launched.push_back(id);
    };

    dock.popup_menu("a.desktop", 100, 200);
    pump_events();
    click_item(dock.get_menu(), 0);

    EXPECT_EQ(launched, (Strings{"a.desktop"}));
}

TEST_F(DockGroupsTest, the_menu_without_an_application_is_not_shown)
{
    dock.popup_menu("", 100, 200);

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, the_menu_stays_in_the_screen)
{
    set({"a.desktop"});
    const XWindowAttributes root = attributes_of(root_window());

    dock.popup_menu("a.desktop", root.width - 5, root.height - 5);
    pump_events();

    const XWindowAttributes menu = attributes_of(dock.get_menu());
    EXPECT_LE(menu.x + menu.width + 2 * menu.border_width, root.width);
    EXPECT_LE(menu.y + menu.height + 2 * menu.border_width, root.height);
}

TEST_F(DockGroupsTest, a_dragged_favorite_shows_where_it_would_go)
{
    set({"a.desktop", "b.desktop", "c.desktop"});
    const ::Window first = icon_at(0);
    press(first, center_of(0));
    move(first, center_of(2) + 1); // behind the last one
    pump_events();

    const XWindowAttributes mark = attributes_of(dock.get_mark());
    EXPECT_EQ(mark.map_state, IsViewable);
    EXPECT_EQ(mark.width, tiny::wm::shell_theme.dock_marker);
    EXPECT_EQ(mark.height, icon_size());
    // in the gap behind the icon c
    const int end_of_c = border() + 3 * (icon_size() + border()) - border();
    EXPECT_EQ(mark.x + tiny::wm::shell_theme.dock_marker / 2,
              end_of_c + border() / 2);
}

TEST_F(DockGroupsTest, the_mark_before_the_first_favorite)
{
    set({"a.desktop", "b.desktop", "c.desktop"});
    const ::Window last = icon_at(2);
    press(last, center_of(2));
    move(last, attributes_of(dock).x);
    pump_events();

    const XWindowAttributes mark = attributes_of(dock.get_mark());
    EXPECT_EQ(mark.map_state, IsViewable);
    EXPECT_EQ(mark.x + tiny::wm::shell_theme.dock_marker / 2,
              border() - border() / 2);
}

TEST_F(DockGroupsTest, the_mark_between_two_favorites)
{
    set({"a.desktop", "b.desktop", "c.desktop"});
    const ::Window first = icon_at(0);
    press(first, center_of(0));
    // behind b, before c
    move(first, center_of(1) + 1);
    pump_events();

    const XWindowAttributes mark = attributes_of(dock.get_mark());
    const int end_of_b = border() + 2 * (icon_size() + border()) - border();
    const int start_of_c = border() + 2 * (icon_size() + border());
    EXPECT_EQ(mark.x + tiny::wm::shell_theme.dock_marker / 2,
              (end_of_b + start_of_c) / 2);
}

TEST_F(DockGroupsTest, the_drop_tells_the_new_place_and_hides_the_mark)
{
    set({"a.desktop", "b.desktop", "c.desktop"});
    std::vector<std::pair<std::string, size_t>> moved;
    dock.on_move_favorite = [&moved](const std::string& id, size_t place) {
        moved.emplace_back(id, place);
    };
    const ::Window first = icon_at(0);
    press(first, center_of(0));
    move(first, center_of(2) + 1);
    release(first, center_of(2) + 1);

    ASSERT_EQ(moved.size(), 1u);
    EXPECT_EQ(moved[0].first, "a.desktop");
    EXPECT_EQ(moved[0].second, 2u);
    EXPECT_EQ(attributes_of(dock.get_mark()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, a_drop_at_the_start_is_the_place_zero)
{
    set({"a.desktop", "b.desktop"});
    std::vector<size_t> places;
    dock.on_move_favorite = [&places](const std::string&, size_t place) {
        places.push_back(place);
    };
    const ::Window second = icon_at(1);
    press(second, center_of(1));
    move(second, 0);
    release(second, 0);

    EXPECT_EQ(places, (std::vector<size_t>{0}));
}

TEST_F(DockGroupsTest, a_drop_at_the_old_place_changes_nothing)
{
    set({"a.desktop", "b.desktop"});
    int moved = 0;
    dock.on_move_favorite = [&moved](const std::string&, size_t) { ++moved; };
    const ::Window first = icon_at(0);

    press(first, center_of(0));
    move(first, center_of(0) + 2 * tiny::wm::shell_theme.dock_drag);
    release(first, center_of(0) + 2 * tiny::wm::shell_theme.dock_drag);

    EXPECT_EQ(moved, 0);
}

TEST_F(DockGroupsTest, the_dock_is_not_rebuilt_under_a_dragged_icon)
{
    set({"a.desktop", "b.desktop"});
    const ::Window first = icon_at(0);
    press(first, center_of(0));
    move(first, center_of(1) + 1);
    FakeClient c;

    add(manage_app(c, "x.desktop")); // would delete the dragged icon

    EXPECT_EQ(icon_at(0), first);
    release(first, center_of(1) + 1);
    EXPECT_EQ(mapped_children_of(dock.get_window()).size(), 5u);
}

TEST_F(DockGroupsTest, a_window_of_another_application_keeps_the_menu_open)
{
    FakeClient c1;
    FakeClient c2;
    add(manage_app(c1, "a.desktop"));
    WmWindow* other = manage_app(c2, "b.desktop");
    add(other);
    right_click(icon_at(0));

    remove(other);

    EXPECT_EQ(attributes_of(dock.get_menu()).map_state, IsViewable);
}

TEST_F(DockGroupsTest, the_menu_is_above_the_catcher_of_the_submenu)
{
    FakeClient c;
    add(manage_app(c, "a.desktop"));
    right_click(icon_at(0));
    click_item(dock.get_menu(), 1);

    const auto stack = children_of(root_window()); // bottom to top
    const auto at = [&](::Window w) {
        return std::find(stack.begin(), stack.end(), w) - stack.begin();
    };
    EXPECT_GT(at(dock.get_windows_menu().get_window()),
              at(dock.get_menu().get_window()));
}

TEST_F(DockGroupsTest, a_click_on_a_favorite_is_not_a_move)
{
    set({"a.desktop"});
    int moved = 0;
    dock.on_move_favorite = [&moved](const std::string&, size_t) { ++moved; };
    Strings launched;
    dock.on_launch = [&launched](const std::string& id) {
        launched.push_back(id);
    };
    const ::Window first = icon_at(0);

    press(first, center_of(0));
    release(first, center_of(0));

    EXPECT_EQ(moved, 0);
    EXPECT_EQ(launched, (Strings{"a.desktop"}));
}

TEST_F(DockGroupsTest, the_only_favorite_has_no_place_to_go)
{
    set({"a.desktop"});
    const ::Window first = icon_at(0);
    press(first, center_of(0));

    move(first, center_of(0) + 100);

    EXPECT_EQ(attributes_of(dock.get_mark()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest,
       escape_during_a_drag_takes_the_mark_away_and_moves_nothing)
{
    set({"a.desktop", "b.desktop"});
    int moved = 0;
    dock.on_move_favorite = [&moved](const std::string&, size_t) { ++moved; };
    const ::Window first = icon_at(0);
    press(first, center_of(0));
    move(first, center_of(1) + 1);
    ASSERT_NE(attributes_of(dock.get_mark()).map_state, IsUnmapped);

    send(first, key_event(first, XK_Escape));
    release(first, center_of(1) + 1);

    EXPECT_EQ(attributes_of(dock.get_mark()).map_state, IsUnmapped);
    EXPECT_EQ(moved, 0);
}

TEST_F(DockGroupsTest, a_drop_without_a_handler_is_ignored)
{
    set({"a.desktop", "b.desktop"});
    const ::Window first = icon_at(0);

    press(first, center_of(0));
    move(first, center_of(1) + 1);
    release(first, center_of(1) + 1);

    EXPECT_EQ(dock.get_favorites().size(), 2u);
    EXPECT_EQ(attributes_of(dock.get_mark()).map_state, IsUnmapped);
}

TEST_F(DockGroupsTest, an_application_that_is_not_a_favorite_cannot_be_dragged)
{
    set({"a.desktop", "b.desktop"});
    FakeClient c;
    add(manage_app(c, "x.desktop"));
    int moved = 0;
    dock.on_move_favorite = [&moved](const std::string&, size_t) { ++moved; };
    const ::Window other = icon_after_line(2);
    ASSERT_NE(other, 0u);

    press(other, 500);
    move(other, 0);
    release(other, 0);

    EXPECT_EQ(moved, 0);
    EXPECT_EQ(attributes_of(dock.get_mark()).map_state, IsUnmapped);
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
        ASSERT_EQ(children_of(dock_window).size(), 3u); // the mark too
    }

    const XErrors errors;
    XWindowAttributes attrs;
    EXPECT_EQ(XGetWindowAttributes(tiny::get_display(), dock_window, &attrs),
              0);
}

} // namespace
