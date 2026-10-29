#include "helpers.h"

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include <X11/keysym.h>

#include "wm_run.h"

static XEvent key_event(const tiny::Widget& w, KeySym key, int type = KeyPress)
{
    XEvent e = make_event(type);
    e.xkey.display = tiny::get_display();
    e.xkey.window = w.get_window();
    e.xkey.root = root_window();
    e.xkey.keycode = XKeysymToKeycode(tiny::get_display(), key);
    return e;
}

static void type_key(const tiny::Widget& w, KeySym key)
{
    send(w.get_window(), key_event(w, key));
}

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

class EntryTest : public ::testing::Test {
  protected:
    WidgetFont font;
    wm::Entry entry{100, 30};
    Recorder exited;

    void SetUp() override
    {
        entry.on_exit.connect(&exited, exited.slot());
        entry.realize(root_window(), 0, 0);
        entry.set_events();
    }

    unsigned int drawn()
    {
        return Image(entry.get_window(), 100, 30)
                .count_not(tiny::theme.widget.normal_bg, 30);
    }
};

TEST_F(EntryTest, realize_creates_window)
{
    const XWindowAttributes attrs = attributes_of(entry);

    EXPECT_EQ(attrs.width, 100);
    EXPECT_EQ(attrs.height, 30);
    EXPECT_TRUE(entry.get_realized());
}

TEST_F(EntryTest, set_events_selects_keyboard_and_focus_events)
{
    const long mask = attributes_of(entry).your_event_mask;

    EXPECT_TRUE(mask & ExposureMask);
    EXPECT_TRUE(mask & KeyPressMask);
    EXPECT_TRUE(mask & FocusChangeMask);
}

TEST_F(EntryTest, empty_entry_draws_nothing)
{
    entry.map();
    send(entry, Expose);

    EXPECT_EQ(drawn(), 0u);
}

TEST_F(EntryTest, typed_text_is_drawn)
{
    entry.map();
    send(entry, FocusIn);

    type_key(entry, XK_a);

    EXPECT_GT(drawn(), 0u);
    EXPECT_EQ(exited.calls, 0);
}

TEST_F(EntryTest, backspace_removes_the_last_character)
{
    entry.map();
    send(entry, FocusIn);
    type_key(entry, XK_a);
    ASSERT_GT(drawn(), 0u);

    type_key(entry, XK_BackSpace);

    EXPECT_EQ(drawn(), 0u);
}

TEST_F(EntryTest, backspace_in_empty_entry_does_nothing)
{
    entry.map();
    send(entry, FocusIn);

    type_key(entry, XK_BackSpace);

    EXPECT_EQ(drawn(), 0u);
    EXPECT_EQ(exited.calls, 0);
}

TEST_F(EntryTest, keys_without_text_are_ignored)
{
    entry.map();
    send(entry, FocusIn);

    type_key(entry, XK_Delete);
    type_key(entry, XK_Tab);

    EXPECT_EQ(drawn(), 0u);
    EXPECT_EQ(exited.calls, 0);
}

TEST_F(EntryTest, escape_signals_exit)
{
    type_key(entry, XK_Escape);

    EXPECT_EQ(exited.calls, 1);
    EXPECT_EQ(exited.last_owner, &entry);
}

TEST_F(EntryTest, clear_removes_the_text)
{
    entry.map();
    send(entry, FocusIn);
    type_key(entry, XK_a);
    ASSERT_GT(drawn(), 0u);

    entry.clear();
    send(entry, Expose);

    EXPECT_EQ(drawn(), 0u);
}

TEST_F(EntryTest, return_runs_the_typed_command_and_signals_exit)
{
    auto marker = std::filesystem::temp_directory_path() /
                  ("tiny-test-run-" + std::to_string(getpid()));
    std::filesystem::remove(marker);
    entry.map();
    send(entry, FocusIn);

    // "touch" + marker path, typed key by key
    const std::string command = "touch " + marker.string();
    for (const char c : command) {
        const KeySym sym = c == ' '   ? XK_space
                           : c == '/' ? XK_slash
                           : c == '-' ? XK_minus
                           : c == '.' ? XK_period
                           : c == '_' ? XK_underscore
                                      : static_cast<KeySym>(c);
        XEvent e = key_event(entry, sym);
        if (c == '_') {
            e.xkey.state = ShiftMask;
        }
        send(entry.get_window(), e);
    }
    type_key(entry, XK_Return);

    EXPECT_EQ(exited.calls, 1);
    EXPECT_TRUE(appears(marker));
    std::filesystem::remove(marker);
}

TEST_F(EntryTest, focus_events_do_not_break_the_entry)
{
    send(entry, FocusIn);
    send(entry, FocusOut);
}

TEST_F(EntryTest, set_focus_gives_the_entry_the_keyboard_focus)
{
    entry.map();
    XSync(tiny::get_display(), False);

    entry.set_focus();

    ::Window focus;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_EQ(focus, entry.get_window());
}

TEST(EntryLifeTest, destructor_disconnects_key_handler)
{
    ::Window window;
    {
        wm::Entry entry(100, 30);
        entry.realize(root_window(), 0, 0);
        entry.set_events();
        window = entry.get_window();
        ASSERT_TRUE(tiny::handlers.count({KeyPress, window}));
    }

    EXPECT_FALSE(tiny::handlers.count({KeyPress, window}));
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

TEST_F(RunDialogTest, realize_contains_the_entry)
{
    wm::RunDialog dialog;

    dialog.realize(root_window());

    EXPECT_EQ(children_of(dialog.get_window()).size(), 1u);
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
    ASSERT_EQ(children_of(dialog.get_window()).size(), 1u);
    EXPECT_EQ(focus, children_of(dialog.get_window())[0]);
}

TEST_F(RunDialogTest, escape_in_the_entry_hides_the_dialog)
{
    wm::RunDialog dialog;
    dialog.realize(root_window());
    dialog.popup();
    const ::Window entry = children_of(dialog.get_window())[0];

    XEvent e = make_event(KeyPress);
    e.xkey.display = tiny::get_display();
    e.xkey.window = entry;
    e.xkey.keycode = XKeysymToKeycode(tiny::get_display(), XK_Escape);
    send(entry, e);

    EXPECT_EQ(attributes_of(dialog).map_state, IsUnmapped);
}
