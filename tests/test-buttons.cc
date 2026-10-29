#include <string>

#include <gtest/gtest.h>

#include "buttons.h"
#include "containers.h"
#include "display.h"
#include "object.h"

namespace tiny {
extern Handlers handlers;
}

// Counts the signals of a button.
class Recorder: public tiny::Object {
  public:
    void on_signal(tiny::Object *owner, const XEvent &, void *)
    {
        ++calls;
        last_owner = owner;
    }

    tiny::object_signal_t slot()
    { return static_cast<tiny::object_signal_t>(&Recorder::on_signal); }

    int calls = 0;
    tiny::Object *last_owner = nullptr;
};

static XEvent make_event(int type)
{
    XEvent e = {};
    e.type = type;
    return e;
}

class ButtonTest: public ::testing::Test {
  protected:
    ::Window root() const
    { return XDefaultRootWindow(tiny::get_display()); }

    void send(const tiny::Widget &w, int type)
    {
        tiny::handlers.call_hanlder({type, w.get_window()}, make_event(type));
    }

    // Drawing needs a font; theme.widget is not initialized by default.
    // The font is opened for the test and closed with the display still open.
    void SetUp() override
    {
        original = tiny::theme.widget;
        with_font = original;
        with_font.init();
        tiny::theme.widget = with_font;
    }

    void TearDown() override
    { tiny::theme.widget = original; }

    uint32_t pixel(const tiny::Widget &w, int x, int y)
    {
        XSync(tiny::get_display(), False);
        XImage *img = XGetImage(tiny::get_display(), w.get_window(), x, y, 1,
                1, AllPlanes, ZPixmap);
        uint32_t px = XGetPixel(img, 0, 0);
        XDestroyImage(img);
        return px;
    }

    // true when anything but the background color is drawn
    bool has_text(const tiny::Widget &w, uint32_t bg)
    {
        XSync(tiny::get_display(), False);
        XImage *img = XGetImage(tiny::get_display(), w.get_window(), 0, 0,
                w.get_width(), w.get_height(), AllPlanes, ZPixmap);
        bool found = false;
        for (uint32_t y = 0; y < w.get_height() && !found; ++y) {
            for (uint32_t x = 0; x < w.get_width() && !found; ++x) {
                found = (XGetPixel(img, x, y) & 0xFFFFFF) != bg;
            }
        }
        XDestroyImage(img);
        return found;
    }

    tiny::Style original;
    tiny::Style with_font;
};

TEST_F(ButtonTest, enter_and_leave_emit_signals)
{
    tiny::Button b(50, 20);
    Recorder enter, leave;
    b.on_enter.connect(&enter, enter.slot());
    b.on_leave.connect(&leave, leave.slot());
    b.realize(root(), 0, 0);

    send(b, EnterNotify);
    EXPECT_EQ(enter.calls, 1);
    EXPECT_EQ(enter.last_owner, &b);
    EXPECT_EQ(leave.calls, 0);

    send(b, LeaveNotify);
    EXPECT_EQ(leave.calls, 1);
}

TEST_F(ButtonTest, click_is_emitted_only_while_hovering)
{
    tiny::Button b(50, 20);
    Recorder click;
    b.on_click.connect(&click, click.slot());
    b.realize(root(), 0, 0);

    send(b, ButtonRelease);
    EXPECT_EQ(click.calls, 0);

    send(b, EnterNotify);
    send(b, ButtonRelease);
    EXPECT_EQ(click.calls, 1);
    EXPECT_EQ(click.last_owner, &b);

    send(b, LeaveNotify);
    send(b, ButtonRelease);
    EXPECT_EQ(click.calls, 1);
}

TEST_F(ButtonTest, click_without_connected_signal_is_ignored)
{
    tiny::Button b(50, 20);
    b.realize(root(), 0, 0);

    send(b, EnterNotify);
    send(b, ButtonRelease);   // must not crash
}

TEST_F(ButtonTest, destructor_disconnects_handlers)
{
    ::Window window;
    {
        tiny::Button b(50, 20);
        b.realize(root(), 0, 0);
        window = b.get_window();
        EXPECT_EQ(tiny::handlers.count({EnterNotify, window}), 1u);
        EXPECT_EQ(tiny::handlers.count({LeaveNotify, window}), 1u);
        EXPECT_EQ(tiny::handlers.count({ButtonRelease, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({EnterNotify, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({LeaveNotify, window}), 0u);
    EXPECT_EQ(tiny::handlers.count({ButtonRelease, window}), 0u);
}

TEST_F(ButtonTest, delete_unrealized_button_through_base_pointer)
{
    tiny::Widget *b = new tiny::Button(50, 20);

    delete b;
}

TEST_F(ButtonTest, label_button_keeps_text)
{
    tiny::LabelButton b(100, 28, "Hello");

    EXPECT_EQ(b.get_text(), "Hello");

    b.set_text("World");
    EXPECT_EQ(b.get_text(), "World");
}

TEST_F(ButtonTest, label_button_draws_text_when_exposed)
{
    tiny::LabelButton b(100, 28, "Menu Item");
    b.realize(root(), 0, 0);
    b.map();

    send(b, Expose);

    EXPECT_TRUE(has_text(b, tiny::theme.widget.normal_bg));
}

TEST_F(ButtonTest, label_button_without_text_draws_nothing)
{
    tiny::LabelButton b(100, 28, "");
    b.realize(root(), 0, 0);
    b.map();

    send(b, Expose);

    EXPECT_FALSE(has_text(b, tiny::theme.widget.normal_bg));
}

TEST_F(ButtonTest, set_text_redraws_mapped_button)
{
    tiny::LabelButton b(100, 28, "");
    b.realize(root(), 0, 0);
    b.map();
    send(b, Expose);
    ASSERT_FALSE(has_text(b, tiny::theme.widget.normal_bg));

    b.set_text("Menu Item");

    EXPECT_TRUE(has_text(b, tiny::theme.widget.normal_bg));
}

TEST_F(ButtonTest, hover_changes_background)
{
    tiny::LabelButton b(100, 28, "x");
    b.realize(root(), 0, 0);
    b.map();
    send(b, Expose);
    EXPECT_EQ(pixel(b, 0, 0), tiny::theme.widget.normal_bg);

    send(b, EnterNotify);
    EXPECT_EQ(pixel(b, 0, 0), tiny::theme.widget.hover_bg);

    send(b, LeaveNotify);
    EXPECT_EQ(pixel(b, 0, 0), tiny::theme.widget.normal_bg);
}

TEST_F(ButtonTest, label_button_destructor_disconnects_expose)
{
    ::Window window;
    {
        tiny::LabelButton b(100, 28, "x");
        b.realize(root(), 0, 0);
        window = b.get_window();
        EXPECT_EQ(tiny::handlers.count({Expose, window}), 1u);
    }

    EXPECT_EQ(tiny::handlers.count({Expose, window}), 0u);
}

TEST_F(ButtonTest, delete_label_button_through_base_pointer)
{
    tiny::Widget *b = new tiny::LabelButton(100, 28, "x");

    delete b;
}

TEST_F(ButtonTest, delete_menu_button_through_base_pointer)
{
    tiny::Widget *b = new tiny::MenuButton(100, 28, "x");

    delete b;
}

class MenuButtonTest: public ButtonTest {
  protected:
    XEvent click_event(int x_root, int y_root, int x, int y)
    {
        XEvent e = make_event(ButtonRelease);
        e.xbutton.x_root = x_root;
        e.xbutton.y_root = y_root;
        e.xbutton.x = x;
        e.xbutton.y = y;
        return e;
    }

    void click(const tiny::Widget &w, const XEvent &e)
    {
        send(w, EnterNotify);
        tiny::handlers.call_hanlder({ButtonRelease, w.get_window()}, e);
    }
};

TEST_F(MenuButtonTest, click_pops_popover_up_under_button)
{
    tiny::MenuButton b(100, 28, "Menu");
    tiny::Popover popover(60, 40);
    b.realize(root(), 0, 0);
    popover.realize(root(), 0, 0);
    b.set_popover(&popover);

    // button at root (300, 200), click inside at (10, 5)
    click(b, click_event(310, 205, 10, 5));

    EXPECT_TRUE(popover.get_maped());
    XSync(tiny::get_display(), False);
    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), popover.get_window(), &attrs);
    // centered under the button: 300 + 100/2 - 60/2, below it: 200 + 28
    EXPECT_EQ(attrs.x, 320);
    EXPECT_EQ(attrs.y, 228);
}

TEST_F(MenuButtonTest, click_does_not_popup_already_visible_popover)
{
    tiny::MenuButton b(100, 28, "Menu");
    tiny::Popover popover(60, 40);
    b.realize(root(), 0, 0);
    popover.realize(root(), 0, 0);
    b.set_popover(&popover);
    click(b, click_event(310, 205, 10, 5));

    click(b, click_event(500, 500, 10, 5));

    XSync(tiny::get_display(), False);
    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), popover.get_window(), &attrs);
    EXPECT_EQ(attrs.x, 320);    // not moved
}

TEST_F(MenuButtonTest, click_without_popover_is_ignored)
{
    tiny::MenuButton b(100, 28, "Menu");
    b.realize(root(), 0, 0);

    click(b, click_event(310, 205, 10, 5));   // must not crash
}
