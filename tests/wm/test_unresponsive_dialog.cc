#include "helpers.h"

#include "wm/unresponsive_dialog.h"

class UnresponsiveDialogTest : public ::testing::Test {
  protected:
    WidgetFont font;

    // Children of the dialog: two lines and the buttons wait and quit
    static ::Window child(const tiny::wm::UnresponsiveDialog& dialog,
                          size_t index)
    {
        return children_of(dialog.get_window()).at(index);
    }

    static void click(::Window button)
    {
        send(button, make_event(EnterNotify));
        send(button, make_event(ButtonRelease));
    }
};

TEST_F(UnresponsiveDialogTest, realize_centers_the_dialog_on_the_root_window)
{
    tiny::wm::UnresponsiveDialog dialog;

    dialog.realize(root_window());

    const XWindowAttributes root = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(dialog);
    EXPECT_EQ(attrs.x, root.width / 2 - attrs.width / 2);
    EXPECT_EQ(attrs.y, root.height / 2 - attrs.height / 2);
    EXPECT_TRUE(attrs.override_redirect);
}

TEST_F(UnresponsiveDialogTest, the_dialog_is_hidden_until_popup)
{
    tiny::wm::UnresponsiveDialog dialog;

    dialog.realize(root_window());

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
}

TEST_F(UnresponsiveDialogTest, it_holds_two_lines_and_two_buttons)
{
    tiny::wm::UnresponsiveDialog dialog;
    dialog.realize(root_window());

    EXPECT_EQ(children_of(dialog.get_window()).size(), 4u);
}

TEST_F(UnresponsiveDialogTest, the_buttons_are_under_the_text_and_inside)
{
    tiny::wm::UnresponsiveDialog dialog;
    dialog.realize(root_window());

    const XWindowAttributes dlg = attributes_of(dialog);
    const XWindowAttributes line = attributes_of(child(dialog, 1));
    const XWindowAttributes wait = attributes_of(child(dialog, 2));
    const XWindowAttributes quit = attributes_of(child(dialog, 3));

    EXPECT_LT(line.y + line.height, wait.y);
    EXPECT_EQ(wait.y, quit.y);
    EXPECT_LT(wait.x + wait.width, quit.x);
    EXPECT_LE(quit.x + quit.width + 2 * quit.border_width, dlg.width);
    EXPECT_LE(quit.y + quit.height + 2 * quit.border_width, dlg.height);
}

TEST_F(UnresponsiveDialogTest, popup_shows_the_dialog_with_its_text)
{
    tiny::wm::UnresponsiveDialog dialog;
    dialog.realize(root_window());

    dialog.popup("Editor");
    for (size_t i = 0; i < 4; ++i) {
        send(child(dialog, i), make_event(Expose));
    }

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
    EXPECT_GT(Image(child(dialog, 0), 100, 10)
                      .count_not(tiny::theme.widget.normal_bg, 10),
              0u);
    EXPECT_GT(Image(child(dialog, 1), 100, 10)
                      .count_not(tiny::theme.widget.normal_bg, 10),
              0u);
}

TEST_F(UnresponsiveDialogTest, popup_names_the_other_windows_that_go_away)
{
    tiny::wm::UnresponsiveDialog dialog;
    dialog.realize(root_window());

    dialog.popup("Editor", 1);
    dialog.popup("Editor", 3);

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
}

TEST_F(UnresponsiveDialogTest, the_buttons_wait_and_quit)
{
    tiny::wm::UnresponsiveDialog dialog;
    dialog.realize(root_window());
    int waited = 0;
    int quit = 0;
    dialog.on_wait.connect([&](const XEvent&) { ++waited; });
    dialog.on_quit.connect([&](const XEvent&) { ++quit; });
    dialog.popup("Editor");

    click(child(dialog, 2));
    EXPECT_EQ(waited, 1);
    EXPECT_EQ(quit, 0);
    click(child(dialog, 3));

    EXPECT_EQ(waited, 1);
    EXPECT_EQ(quit, 1);
}

TEST_F(UnresponsiveDialogTest, a_long_name_is_cut_inside_the_dialog)
{
    tiny::wm::UnresponsiveDialog dialog;
    dialog.realize(root_window());
    std::string name;
    for (int i = 0; i < 40; ++i) {
        name += "\xc4\x9b"; // a two byte character, 80 bytes in all
    }

    dialog.popup(name);
    dialog.popup(std::string(200, 'x'));

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
}
