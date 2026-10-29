#include <string>

#include <gtest/gtest.h>

#include "display.h"
#include "widget.h"

namespace tiny {
extern Handlers handlers;
}

class TestWidget : public tiny::Widget {
  public:
    using tiny::Widget::get_theme_state;
    using tiny::Widget::Widget;

    void set_selected(bool v) { is_selected = v; }
    void set_hover(bool v) { is_hover = v; }
    void set_focus(bool v) { is_focus = v; }
    void set_pressed(bool v) { is_pressed = v; }
    void set_disabled(bool v) { is_disabled = v; }
};

class Receiver : public tiny::Object {
  public:
    using tiny::Object::connect_window;

    void on_event(const XEvent&, void*) {}

    tiny::event_signal_t slot()
    {
        return static_cast<tiny::event_signal_t>(&Receiver::on_event);
    }
};

class WidgetTest : public ::testing::Test {
  protected:
    ::Window root() const { return XDefaultRootWindow(tiny::get_display()); }

    bool has_configure_handler(::Window w) const
    {
        return tiny::handlers.count({ConfigureNotify, w}) == 1;
    }
};

TEST_F(WidgetTest, constructor_sets_size_and_state)
{
    TestWidget w(30, 20);

    EXPECT_EQ(w.get_width(), 30u);
    EXPECT_EQ(w.get_height(), 20u);
    EXPECT_FALSE(w.get_realized());
    EXPECT_FALSE(w.get_maped());
    EXPECT_EQ(w.get_window(), 0u);
}

TEST_F(WidgetTest, style_comes_from_theme)
{
    TestWidget w(10, 10);

    EXPECT_EQ(&w.get_style(), &tiny::theme.widget);
    EXPECT_EQ(w.get_border(), tiny::theme.widget.border_width);
    EXPECT_EQ(w.get_padding(), tiny::theme.widget.padding_width);
}

TEST_F(WidgetTest, realize_creates_window)
{
    TestWidget w(30, 20);

    w.realize(root(), 5, 6);

    EXPECT_TRUE(w.get_realized());
    EXPECT_NE(w.get_window(), 0u);
    EXPECT_EQ(w.get_parent_window(), root());

    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), w.get_window(), &attrs);
    EXPECT_EQ(attrs.x, 5);
    EXPECT_EQ(attrs.y, 6);
    EXPECT_EQ(attrs.width, 30);
    EXPECT_EQ(attrs.height, 20);
}

TEST_F(WidgetTest, realize_input_widget_creates_input_only_window)
{
    TestWidget w(tiny::Widget::Type::Input, 30, 20);

    w.realize(root(), 0, 0);

    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), w.get_window(), &attrs);
    EXPECT_EQ(attrs.c_class, InputOnly);
}

TEST_F(WidgetTest, realize_transparent_widget_uses_32_bit_visual)
{
    XVisualInfo vinfo;
    if (!XMatchVisualInfo(tiny::get_display(),
                          XDefaultScreen(tiny::get_display()), 32, TrueColor,
                          &vinfo)) {
        GTEST_SKIP() << "X server has no 32 bit visual";
    }
    TestWidget w(tiny::Widget::Type::Transparent, 30, 20);

    w.realize(root(), 0, 0);

    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), w.get_window(), &attrs);
    EXPECT_EQ(attrs.depth, 32);
}

TEST_F(WidgetTest, delete_through_base_pointer_destroys_window)
{
    auto* w = new tiny::Widget(30, 20);
    w->realize(root(), 0, 0);
    ::Window window = w->get_window();

    delete w;

    EXPECT_FALSE(has_configure_handler(window));
}

TEST_F(WidgetTest, realize_twice_reports_error_and_keeps_window)
{
    TestWidget w(30, 20);
    w.realize(root(), 0, 0);
    ::Window window = w.get_window();

    ::testing::internal::CaptureStderr();
    w.realize(root(), 10, 10);
    std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("realized yet"), std::string::npos);
    EXPECT_EQ(w.get_window(), window);
}

TEST_F(WidgetTest, realize_registers_configure_handler_until_destroyed)
{
    ::Window window;
    {
        TestWidget w(30, 20);
        w.realize(root(), 0, 0);
        window = w.get_window();
        EXPECT_TRUE(has_configure_handler(window));
    }

    EXPECT_FALSE(has_configure_handler(window));
}

TEST_F(WidgetTest, destructor_destroys_window)
{
    ::Window window;
    {
        TestWidget w(30, 20);
        w.realize(root(), 0, 0);
        window = w.get_window();
    }
    XSync(tiny::get_display(), False);

    // a destroyed window can not be queried; the error handler would be
    // triggered, so check the tree instead
    ::Window rv_root;
    ::Window rv_parent;
    ::Window* children = nullptr;
    unsigned int count = 0;
    XQueryTree(tiny::get_display(), root(), &rv_root, &rv_parent, &children,
               &count);
    bool found = false;
    for (unsigned int i = 0; i < count; ++i) {
        found = found || children[i] == window;
    }
    if (children) {
        XFree(children);
    }
    EXPECT_FALSE(found);
}

TEST_F(WidgetTest, destroying_unrealized_widget_keeps_other_handlers)
{
    // an unrealized widget has window 0 and must not disconnect anything
    Receiver other;
    other.connect_window(ConfigureNotify, 0, other.slot());
    {
        TestWidget w(30, 20);
    }

    EXPECT_EQ(tiny::handlers.count({ConfigureNotify, 0}), 1u);
}

TEST_F(WidgetTest, map_without_realize_reports_error)
{
    TestWidget w(30, 20);

    ::testing::internal::CaptureStderr();
    w.map();
    std::string out = ::testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("not realized"), std::string::npos);
    EXPECT_FALSE(w.get_maped());
}

TEST_F(WidgetTest, map_and_unmap_change_state)
{
    TestWidget w(30, 20);
    w.realize(root(), 0, 0);

    w.map();
    EXPECT_TRUE(w.get_maped());

    w.unmap();
    EXPECT_FALSE(w.get_maped());
}

TEST_F(WidgetTest, map_all_maps_widget)
{
    TestWidget w(30, 20);
    w.realize(root(), 0, 0);

    w.map_all();

    EXPECT_TRUE(w.get_maped());
}

TEST_F(WidgetTest, unmap_without_map_is_noop)
{
    TestWidget w(30, 20);

    w.unmap();
    EXPECT_FALSE(w.get_maped());

    w.realize(root(), 0, 0);
    w.unmap();
    EXPECT_FALSE(w.get_maped());
}

TEST_F(WidgetTest, resize_before_realize_changes_size_immediately)
{
    TestWidget w(30, 20);

    w.resize(50, 40);

    EXPECT_EQ(w.get_width(), 50u);
    EXPECT_EQ(w.get_height(), 40u);
}

TEST_F(WidgetTest, resize_after_realize_waits_for_configure_notify)
{
    TestWidget w(30, 20);
    w.realize(root(), 0, 0);

    w.resize(50, 40);
    EXPECT_EQ(w.get_width(), 30u);
    EXPECT_EQ(w.get_height(), 20u);

    XEvent e = {};
    e.type = ConfigureNotify;
    e.xconfigure.width = 50;
    e.xconfigure.height = 40;
    tiny::handlers.call_hanlder({ConfigureNotify, w.get_window()}, e);

    EXPECT_EQ(w.get_width(), 50u);
    EXPECT_EQ(w.get_height(), 40u);
}

TEST_F(WidgetTest, theme_state_is_normal_by_default)
{
    TestWidget w(10, 10);

    EXPECT_EQ(w.get_theme_state(), uint8_t(tiny::Style::State::Normal));
}

TEST_F(WidgetTest, theme_state_combines_flags)
{
    using tiny::Style;
    TestWidget w(10, 10);

    w.set_hover(true);
    EXPECT_EQ(w.get_theme_state(),
              uint8_t(Style::State::Normal) | uint8_t(Style::State::Hover));

    w.set_selected(true);
    w.set_focus(true);
    w.set_pressed(true);
    w.set_disabled(true);
    EXPECT_EQ(w.get_theme_state(), uint8_t(Style::State::Normal) |
                                           uint8_t(Style::State::Selected) |
                                           uint8_t(Style::State::Hover) |
                                           uint8_t(Style::State::Focus) |
                                           uint8_t(Style::State::Pressed) |
                                           uint8_t(Style::State::Disabled));
}
