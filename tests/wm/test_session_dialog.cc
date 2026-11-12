#include "wm_helpers.h"

#include "wm/session_dialog.h"

using tiny::wm::SessionKind;

class SessionDialogTest : public ::testing::Test {
  protected:
    WidgetFont font;
    const ShellFonts shell_fonts;

    // The children of the dialog: three lines, then Cancel and the button that agrees
    static ::Window child(const tiny::wm::SessionDialog& dialog, size_t index)
    {
        return children_of(dialog.get_window()).at(index);
    }

    static void click(::Window button)
    {
        send(button, make_event(EnterNotify));
        send(button, make_event(ButtonRelease));
    }
};

TEST_F(SessionDialogTest, the_dialog_is_hidden_until_it_has_something_to_say)
{
    tiny::wm::SessionDialog dialog;

    dialog.realize(root_window());

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
    EXPECT_TRUE(attributes_of(dialog).override_redirect);
    EXPECT_EQ(children_of(dialog.get_window()).size(), 5u);
}

TEST_F(SessionDialogTest, the_question_shows_the_dialog_with_its_text)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());

    dialog.show_question(SessionKind::Logout, 60);
    for (size_t i = 0; i < 5; ++i) {
        send(child(dialog, i), make_event(Expose));
    }

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
    for (const size_t line : {0u, 1u}) {
        EXPECT_GT(Image(child(dialog, line), 100, 10)
                          .count_not(tiny::theme.widget.normal_bg, 10),
                  0u)
                << line;
    }
}

TEST_F(SessionDialogTest, the_dialog_opens_in_the_middle_of_the_monitor)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());
    dialog.set_screen({100, 50, 800, 600});

    dialog.show_question(SessionKind::Restart, 10);

    const XWindowAttributes attrs = attributes_of(dialog);
    EXPECT_EQ(attrs.x, 100 + (800 - attrs.width) / 2);
    EXPECT_EQ(attrs.y, 50 + (600 - attrs.height) / 2);
}

TEST_F(SessionDialogTest, the_countdown_changes_the_text_not_the_place)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());
    dialog.set_screen({0, 0, 800, 600});
    dialog.show_question(SessionKind::Logout, 60);
    XMoveWindow(tiny::get_display(), dialog.get_window(), 7, 9);

    dialog.show_question(SessionKind::Logout, 59);

    EXPECT_EQ(attributes_of(dialog).x, 7);
    EXPECT_EQ(attributes_of(dialog).y, 9);
}

TEST_F(SessionDialogTest, the_buttons_are_under_the_text_and_inside)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());
    dialog.show_question(SessionKind::Logout, 60);

    const XWindowAttributes dlg = attributes_of(dialog);
    const XWindowAttributes line = attributes_of(child(dialog, 2));
    const XWindowAttributes cancel = attributes_of(child(dialog, 3));
    const XWindowAttributes proceed = attributes_of(child(dialog, 4));

    EXPECT_LT(line.y + line.height, cancel.y);
    EXPECT_EQ(cancel.y, proceed.y);
    EXPECT_LT(cancel.x + cancel.width, proceed.x);
    EXPECT_LE(proceed.x + proceed.width + 2 * proceed.border_width, dlg.width);
    EXPECT_LE(proceed.y + proceed.height + 2 * proceed.border_width,
              dlg.height);
}

TEST_F(SessionDialogTest, the_buttons_cancel_and_proceed)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());
    int cancelled = 0;
    int proceeded = 0;
    dialog.on_cancel.connect([&](const XEvent&) { ++cancelled; });
    dialog.on_proceed.connect([&](const XEvent&) { ++proceeded; });
    dialog.show_question(SessionKind::Shutdown, 60);

    click(child(dialog, 3));
    EXPECT_EQ(cancelled, 1);
    EXPECT_EQ(proceeded, 0);
    click(child(dialog, 4));

    EXPECT_EQ(cancelled, 1);
    EXPECT_EQ(proceeded, 1);
}

TEST_F(SessionDialogTest,
       waiting_for_the_applications_says_anyway_on_the_button)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());
    dialog.show_question(SessionKind::Logout, 60);

    dialog.show_waiting(SessionKind::Logout, {"Firefox", "Terminal"});

    // the dialog stays where it is and the text of the button changes
    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
    EXPECT_EQ(attributes_of(child(dialog, 4)).map_state, IsViewable);
}

TEST_F(SessionDialogTest, a_failure_leaves_only_the_button_that_closes)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());

    dialog.show_failure(SessionKind::Restart, "sudo reboot",
                        "failed (exit status 1).");

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
    EXPECT_EQ(attributes_of(child(dialog, 3)).map_state, IsViewable);
    EXPECT_EQ(attributes_of(child(dialog, 4)).map_state, IsUnmapped);
}

TEST_F(SessionDialogTest, a_question_after_a_failure_has_both_buttons_again)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());
    dialog.show_failure(SessionKind::Restart, "x", "failed.");
    dialog.unmap();

    dialog.show_question(SessionKind::Restart, 60);

    EXPECT_EQ(attributes_of(child(dialog, 4)).map_state, IsViewable);
}

TEST_F(SessionDialogTest, a_long_name_is_cut_inside_the_dialog)
{
    tiny::wm::SessionDialog dialog;
    dialog.realize(root_window());

    dialog.show_waiting(
            SessionKind::Logout,
            {std::string(500, 'x'), "\xC4\x8D" + std::string(200, 'y')});

    const XWindowAttributes dlg = attributes_of(dialog);
    const XWindowAttributes line = attributes_of(child(dialog, 1));
    EXPECT_LE(line.x + line.width, dlg.width);
}
