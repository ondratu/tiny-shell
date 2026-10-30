#include "helpers.h"

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "wm_run.h"

// Waits (up to 2 s) for the process started by Return to create `path`.
static bool appears(const std::filesystem::path& path)
{
    for (int i = 0; i < 200; ++i) {
        if (std::filesystem::exists(path)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// ---- RunDialog ----

class RunDialogTest : public ::testing::Test {
  protected:
    WidgetFont font;
};

TEST_F(RunDialogTest, realize_centers_the_dialog_on_the_root_window)
{
    wm::RunDialog dialog;

    dialog.realize(root_window());

    const XWindowAttributes root = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(dialog);
    EXPECT_EQ(attrs.width, 320);
    EXPECT_EQ(attrs.x, root.width / 2 - 320 / 2);
    EXPECT_EQ(attrs.y, root.height / 2 - attrs.height / 2);
}

// The dialog holds a title and an entry, in this order
static ::Window title_of(const wm::RunDialog& dialog)
{
    return children_of(dialog.get_window()).at(0);
}

static ::Window entry_of(const wm::RunDialog& dialog)
{
    return children_of(dialog.get_window()).at(1);
}

TEST_F(RunDialogTest, realize_contains_a_title_and_an_entry)
{
    wm::RunDialog dialog;

    dialog.realize(root_window());

    EXPECT_EQ(children_of(dialog.get_window()).size(), 2u);
}

TEST_F(RunDialogTest, title_is_above_the_entry_and_both_fit_in_the_dialog)
{
    wm::RunDialog dialog;
    dialog.realize(root_window());

    const XWindowAttributes dlg = attributes_of(dialog);
    const XWindowAttributes title = attributes_of(title_of(dialog));
    const XWindowAttributes entry = attributes_of(entry_of(dialog));

    EXPECT_LT(title.y + title.height, entry.y);
    EXPECT_GT(title.x, 0);
    EXPECT_EQ(entry.x, title.x);
    EXPECT_LE(entry.x + entry.width + 2 * entry.border_width, dlg.width);
    EXPECT_LE(entry.y + entry.height + 2 * entry.border_width, dlg.height);
}

TEST_F(RunDialogTest, title_says_what_the_dialog_does)
{
    wm::RunDialog dialog;
    dialog.realize(root_window());
    dialog.popup();
    send(title_of(dialog), make_event(Expose));

    // the title draws text
    EXPECT_GT(Image(title_of(dialog), 100, 10)
                      .count_not(tiny::theme.widget.normal_bg, 10),
              0u);
}

TEST_F(RunDialogTest, dialog_is_hidden_until_popup)
{
    wm::RunDialog dialog;

    dialog.realize(root_window());

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
}

TEST_F(RunDialogTest, popup_shows_the_dialog_and_focuses_the_entry)
{
    wm::RunDialog dialog;
    dialog.realize(root_window());

    dialog.popup();

    EXPECT_EQ(attributes_of(dialog).map_state, IsViewable);
    ::Window focus;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_EQ(focus, entry_of(dialog));
}

TEST_F(RunDialogTest, escape_in_the_entry_hides_the_dialog)
{
    wm::RunDialog dialog;
    dialog.realize(root_window());
    dialog.popup();

    type_key(entry_of(dialog), XK_Escape);

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
}

TEST_F(RunDialogTest, return_runs_the_typed_command_and_hides_the_dialog)
{
    const auto marker = std::filesystem::temp_directory_path() /
                        ("tiny-test-run-" + std::to_string(getpid()));
    std::filesystem::remove(marker);
    wm::RunDialog dialog;
    dialog.realize(root_window());
    dialog.popup();

    type_text(entry_of(dialog), "touch " + marker.string());
    type_key(entry_of(dialog), XK_Return);

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
    EXPECT_TRUE(appears(marker));
    std::filesystem::remove(marker);
}

TEST_F(RunDialogTest, return_in_an_empty_entry_only_hides_the_dialog)
{
    wm::RunDialog dialog;
    dialog.realize(root_window());
    dialog.popup();

    type_key(entry_of(dialog), XK_Return);

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
}

TEST_F(RunDialogTest, hidden_dialog_forgets_the_typed_text)
{
    const auto marker = std::filesystem::temp_directory_path() /
                        ("tiny-test-forget-" + std::to_string(getpid()));
    std::filesystem::remove(marker);
    wm::RunDialog dialog;
    dialog.realize(root_window());
    dialog.popup();
    type_text(entry_of(dialog), "touch " + marker.string());
    type_key(entry_of(dialog), XK_Escape);
    dialog.popup();

    type_key(entry_of(dialog), XK_Return); // nothing is left to run

    EXPECT_FALSE(appears(marker));
}
