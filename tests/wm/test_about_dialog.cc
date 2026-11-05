#include "wm_helpers.h"

#include "wm/about_dialog.h"
#include "wm/shell_theme.h"

class AboutDialogTest : public ::testing::Test {
  protected:
    WidgetFont font;
};

TEST_F(AboutDialogTest, realize_centers_the_dialog_on_the_root_window)
{
    tiny::wm::AboutDialog dialog;

    dialog.realize(root_window());

    const XWindowAttributes root = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(dialog);
    EXPECT_EQ(attrs.x, root.width / 2 - attrs.width / 2);
    EXPECT_EQ(attrs.y, root.height / 2 - attrs.height / 2);
    EXPECT_EQ(attrs.map_state, IsUnmapped);
}

TEST_F(AboutDialogTest, popup_shows_the_version_and_the_system)
{
    tiny::wm::AboutDialog dialog;
    dialog.realize(root_window());

    dialog.popup("1.2.3", {"Test Linux 9", "Linux 6.1", "x86_64"});

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
    EXPECT_EQ(dialog.line(0), "Tiny Shell");
    EXPECT_EQ(dialog.line(2), "Version 1.2.3");
    EXPECT_EQ(dialog.line(3), "Test Linux 9");
    EXPECT_EQ(dialog.line(4), "Linux 6.1 (x86_64)");
}

TEST_F(AboutDialogTest, an_unknown_system_is_told)
{
    tiny::wm::AboutDialog dialog;
    dialog.realize(root_window());

    dialog.popup("x", {});

    EXPECT_EQ(dialog.line(3), "System unknown");
    EXPECT_EQ(dialog.line(4), "");
}

TEST_F(AboutDialogTest, a_kernel_without_a_machine_has_no_brackets)
{
    tiny::wm::AboutDialog dialog;
    dialog.realize(root_window());

    dialog.popup("x", {"S", "NetBSD 10", ""});

    EXPECT_EQ(dialog.line(4), "NetBSD 10");
}

TEST_F(AboutDialogTest, a_line_out_of_range_is_empty)
{
    const tiny::wm::AboutDialog dialog;

    EXPECT_EQ(dialog.line(-1), "");
    EXPECT_EQ(dialog.line(5), "");
}

TEST_F(AboutDialogTest, the_button_close_hides_the_dialog)
{
    tiny::wm::AboutDialog dialog;
    dialog.realize(root_window());
    dialog.popup("x", {});
    const ::Window button = children_of(dialog.get_window()).back();

    send(button, [&] {
        XEvent e = make_event(EnterNotify);
        e.xcrossing.window = button;
        return e;
    }());
    send(button, [&] {
        XEvent e = make_event(ButtonRelease);
        e.xbutton.window = button;
        e.xbutton.button = Button1;
        return e;
    }());

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
}
