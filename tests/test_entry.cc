#include "helpers.h"

#include "entry.h"

class EntryTest : public ::testing::Test {
  protected:
    WidgetFont font;
    tiny::Entry entry{100, 30};
    Recorder changed;
    Recorder activated;
    Recorder cancelled;
    Recorder keyed;

    void SetUp() override
    {
        entry.on_change.connect(changed.slot());
        entry.on_activate.connect(activated.slot());
        entry.on_cancel.connect(cancelled.slot());
        entry.on_key.connect(keyed.slot());
        entry.realize(root_window(), 0, 0);
        entry.set_events();
    }

    void focus()
    {
        entry.map();
        send(entry, FocusIn);
    }

    Image image() { return Image(entry.get_window(), 100, 30); }

    // Pixels inside the entry that are not its background
    unsigned int drawn() { return image().count_not(background(), 30); }

    static uint32_t background()
    {
        return tiny::theme.entry.get_bg(
                static_cast<uint8_t>(tiny::Style::State::Normal));
    }
};

TEST_F(EntryTest, realize_creates_window_of_the_given_size)
{
    const XWindowAttributes attrs = attributes_of(entry);

    EXPECT_EQ(attrs.width, 100);
    EXPECT_EQ(attrs.height, 30);
    EXPECT_TRUE(entry.get_realized());
}

TEST_F(EntryTest, entry_uses_the_entry_style)
{
    EXPECT_EQ(&entry.get_style(), &tiny::theme.entry);
}

TEST_F(EntryTest, set_events_selects_keyboard_and_focus_events)
{
    const long mask = attributes_of(entry).your_event_mask;

    EXPECT_TRUE(mask & ExposureMask);
    EXPECT_TRUE(mask & KeyPressMask);
    EXPECT_TRUE(mask & FocusChangeMask);
}

TEST_F(EntryTest, empty_entry_without_focus_draws_nothing)
{
    entry.map();
    send(entry, Expose);

    EXPECT_EQ(drawn(), 0u);
}

TEST_F(EntryTest, entry_background_is_dark_gray)
{
    entry.map();
    send(entry, Expose);

    EXPECT_EQ(image().at(50, 15), 0x2E3436u);
}

TEST_F(EntryTest, focused_entry_draws_the_cursor)
{
    focus();

    EXPECT_GT(drawn(), 0u);
}

TEST_F(EntryTest, focus_changes_the_border_color)
{
    focus();
    send(entry, FocusIn);
    XSync(tiny::get_display(), False);
    const uint32_t focused = tiny::theme.entry.get_br(
            static_cast<uint8_t>(tiny::Style::State::Normal) |
            static_cast<uint8_t>(tiny::Style::State::Focus));
    const uint32_t normal = tiny::theme.entry.get_br(
            static_cast<uint8_t>(tiny::Style::State::Normal));

    EXPECT_NE(focused, normal);
}

TEST_F(EntryTest, losing_the_focus_removes_the_cursor)
{
    focus();
    ASSERT_GT(drawn(), 0u);

    send(entry, FocusOut);

    EXPECT_EQ(drawn(), 0u);
}

static XEvent focus_event(const tiny::Entry& entry, int type, int detail)
{
    XEvent e = make_event(type);
    e.xfocus.window = entry.get_window();
    e.xfocus.detail = detail;
    return e;
}

TEST_F(EntryTest, focus_of_the_pointer_window_is_not_the_focus_of_the_entry)
{
    entry.map();

    // the pointer is over the entry while the focus is somewhere else
    send(entry.get_window(), focus_event(entry, FocusIn, NotifyPointer));

    EXPECT_EQ(drawn(), 0u);
}

TEST_F(EntryTest, pointer_focus_out_does_not_take_the_focus_from_the_entry)
{
    focus();
    ASSERT_GT(drawn(), 0u);

    send(entry.get_window(), focus_event(entry, FocusOut, NotifyPointer));

    EXPECT_GT(drawn(), 0u);
}

TEST_F(EntryTest, repeated_focus_in_does_not_redraw_the_entry)
{
    focus();
    // a mark that only a redraw would clear
    GC gc = XCreateGC(tiny::get_display(), entry.get_window(), 0, nullptr);
    XSetForeground(tiny::get_display(), gc, 0xFF0000);
    XFillRectangle(tiny::get_display(), entry.get_window(), gc, 80, 2, 10, 10);
    ASSERT_EQ(image().at(85, 5), 0xFF0000u);

    send(entry.get_window(), focus_event(entry, FocusIn, NotifyAncestor));

    EXPECT_EQ(image().at(85, 5), 0xFF0000u);
    XFreeGC(tiny::get_display(), gc);
}

TEST_F(EntryTest, typed_text_is_stored_drawn_and_reported)
{
    focus();
    const unsigned int cursor_only = drawn();

    type_text(entry, "ab");

    EXPECT_EQ(entry.get_text(), "ab");
    EXPECT_GT(drawn(), cursor_only);
    EXPECT_EQ(changed.calls, 2);
    EXPECT_EQ(changed.last_owner, &entry);
    EXPECT_EQ(activated.calls, 0);
    EXPECT_EQ(cancelled.calls, 0);
}

TEST_F(EntryTest, shifted_key_types_the_shifted_character)
{
    focus();

    type_text(entry, "A_");

    EXPECT_EQ(entry.get_text(), "A_");
}

TEST_F(EntryTest, control_characters_are_not_text)
{
    focus();

    type_key(entry, XK_a, ControlMask);

    EXPECT_EQ(entry.get_text(), "");
    EXPECT_EQ(changed.calls, 0);
}

TEST_F(EntryTest, delete_character_is_not_text)
{
    focus();

    type_key(entry, XK_KP_Delete); // gives the character 0x7F

    EXPECT_EQ(entry.get_text(), "");
    EXPECT_EQ(changed.calls, 0);
}

TEST_F(EntryTest, backspace_removes_the_last_character)
{
    focus();
    type_text(entry, "ab");

    type_key(entry, XK_BackSpace);

    EXPECT_EQ(entry.get_text(), "a");
    EXPECT_EQ(changed.calls, 3);
}

TEST_F(EntryTest, backspace_removes_a_whole_utf8_character)
{
    focus();
    entry.set_text("a\xC3\xA1"); // "á" has two bytes

    type_key(entry, XK_BackSpace);

    EXPECT_EQ(entry.get_text(), "a");
}

TEST_F(EntryTest, backspace_in_empty_entry_does_nothing)
{
    focus();

    type_key(entry, XK_BackSpace);

    EXPECT_EQ(entry.get_text(), "");
    EXPECT_EQ(changed.calls, 0);
}

TEST_F(EntryTest, keys_without_text_are_ignored)
{
    focus();

    type_key(entry, XK_Delete);
    type_key(entry, XK_Tab);
    type_key(entry, XK_Shift_L);

    EXPECT_EQ(entry.get_text(), "");
    EXPECT_EQ(changed.calls, 0);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(EntryTest, return_signals_activate_and_keeps_the_text)
{
    focus();
    type_text(entry, "ls");

    type_key(entry, XK_Return);

    EXPECT_EQ(activated.calls, 1);
    EXPECT_EQ(activated.last_owner, &entry);
    EXPECT_EQ(entry.get_text(), "ls");
    EXPECT_EQ(cancelled.calls, 0);
}

TEST_F(EntryTest, keys_that_move_in_a_list_or_a_grid_are_signaled_not_typed)
{
    focus();
    entry.pass_horizontal_keys();
    type_text(entry, "ab");

    for (const int sym :
         {XK_Left, XK_Right, XK_Up, XK_Down, XK_Page_Up, XK_Page_Down}) {
        type_key(entry, static_cast<KeySym>(sym));
    }

    EXPECT_EQ(keyed.calls, 6);
    EXPECT_EQ(keyed.last_owner, &entry);
    EXPECT_EQ(entry.get_text(), "ab");
    EXPECT_EQ(changed.calls, 2); // only the two letters
}

TEST_F(EntryTest, escape_signals_cancel)
{
    type_key(entry, XK_Escape);

    EXPECT_EQ(cancelled.calls, 1);
    EXPECT_EQ(cancelled.last_owner, &entry);
    EXPECT_EQ(activated.calls, 0);
}

TEST_F(EntryTest, set_text_and_clear_change_the_text)
{
    focus();
    entry.set_text("abc");
    EXPECT_EQ(entry.get_text(), "abc");
    const unsigned int with_text = drawn();

    entry.clear();

    EXPECT_EQ(entry.get_text(), "");
    EXPECT_LT(drawn(), with_text);
    EXPECT_EQ(changed.calls, 0); // only edits by the user are reported
}

TEST_F(EntryTest, long_text_shows_its_end)
{
    focus();

    entry.set_text(std::string(60, 'm'));

    // the cursor after the last character stays inside the entry
    const Image img = image();
    unsigned int right_edge = 0;
    for (int y = 0; y < 30; ++y) {
        right_edge += img.at(99 - 1, y) != background();
        right_edge += img.at(99 - 2, y) != background();
    }
    EXPECT_EQ(right_edge, 0u);
    EXPECT_GT(drawn(), 0u);
}

TEST_F(EntryTest, set_focus_gives_the_entry_the_keyboard_focus)
{
    entry.map();
    XSync(tiny::get_display(), False);

    entry.set_focus();

    ::Window focus_window;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus_window, &revert);
    EXPECT_EQ(focus_window, entry.get_window());
}

TEST(EntryInputMethodTest, entry_works_without_an_input_method)
{
    const WidgetFont font;
    const char* old = getenv("XMODIFIERS");
    const std::string saved = old ? old : "";
    setenv("XMODIFIERS", "@im=tiny-test-missing", 1);
    tiny::Entry entry(100, 30);
    entry.realize(root_window(), 0, 0);
    entry.set_events();
    entry.map();
    send(entry, FocusIn);
    if (old) {
        setenv("XMODIFIERS", saved.c_str(), 1);
    } else {
        unsetenv("XMODIFIERS");
    }

    type_text(entry, "ab");
    type_key(entry, XK_BackSpace);

    EXPECT_EQ(entry.get_text(), "a");
}

TEST(EntryLifeTest, destructor_disconnects_the_handlers)
{
    const WidgetFont font;
    ::Window window;
    {
        tiny::Entry entry(100, 30);
        entry.realize(root_window(), 0, 0);
        entry.set_events();
        window = entry.get_window();
        ASSERT_TRUE(tiny::handlers.count({KeyPress, window}));
        ASSERT_TRUE(tiny::handlers.count({Expose, window}));
    }

    EXPECT_FALSE(tiny::handlers.count({KeyPress, window}));
    EXPECT_FALSE(tiny::handlers.count({Expose, window}));
    EXPECT_FALSE(tiny::handlers.count({FocusIn, window}));
    EXPECT_FALSE(tiny::handlers.count({FocusOut, window}));
}

// Cursor, paste and copy

class EntryEditTest : public EntryTest {
  protected:
    void SetUp() override
    {
        EntryTest::SetUp();
        focus();
    }

    static XEvent button(const tiny::Entry& e, unsigned number, int x = 5)
    {
        XEvent ev = make_event(ButtonPress);
        ev.xbutton.window = e.get_window();
        ev.xbutton.button = number;
        ev.xbutton.x = x;
        return ev;
    }

    void settle()
    {
        for (int i = 0; i < 5; ++i) {
            pump_events();
        }
    }
};

TEST_F(EntryEditTest, left_moves_the_cursor_and_typing_inserts_there)
{
    entry.set_text("ac");
    type_key(entry, XK_Left);
    type_key(entry, XK_b);

    EXPECT_EQ(entry.get_text(), "abc");
    EXPECT_EQ(entry.get_cursor(), 2u);
}

TEST_F(EntryEditTest, moving_the_cursor_is_not_a_change_of_the_text)
{
    entry.set_text("ab");
    type_key(entry, XK_Left);
    type_key(entry, XK_Home);
    type_key(entry, XK_End);
    type_key(entry, XK_Right);

    EXPECT_EQ(changed.calls, 0);
}

TEST_F(EntryEditTest, home_and_end_move_the_cursor)
{
    entry.set_text("abc");
    type_key(entry, XK_Home);
    EXPECT_EQ(entry.get_cursor(), 0u);
    type_key(entry, XK_End);
    EXPECT_EQ(entry.get_cursor(), 3u);
}

TEST_F(EntryEditTest, delete_removes_the_character_after_the_cursor)
{
    entry.set_text("abc");
    type_key(entry, XK_Home);
    type_key(entry, XK_Delete);

    EXPECT_EQ(entry.get_text(), "bc");
    EXPECT_EQ(changed.calls, 1);
}

TEST_F(EntryEditTest, delete_at_the_end_changes_nothing)
{
    entry.set_text("abc");
    type_key(entry, XK_Delete);

    EXPECT_EQ(entry.get_text(), "abc");
    EXPECT_EQ(changed.calls, 0);
}

TEST_F(EntryEditTest, the_arrows_go_to_a_grid_when_asked)
{
    entry.pass_horizontal_keys();
    entry.set_text("ab");
    type_key(entry, XK_Left);

    EXPECT_EQ(keyed.calls, 1);
    EXPECT_EQ(entry.get_cursor(), 2u);
}

TEST_F(EntryEditTest, up_and_down_always_go_to_on_key)
{
    type_key(entry, XK_Up);
    type_key(entry, XK_Down);

    EXPECT_EQ(keyed.calls, 2);
}

TEST_F(EntryEditTest, a_click_puts_the_cursor_between_the_characters)
{
    entry.set_text("abc");
    send(entry.get_window(), button(entry, Button1, 0));
    EXPECT_EQ(entry.get_cursor(), 0u);

    send(entry.get_window(), button(entry, Button1, 90));
    EXPECT_EQ(entry.get_cursor(), 3u);
}

TEST_F(EntryEditTest, copy_and_paste_between_two_entries)
{
    tiny::Entry other(100, 30);
    other.realize(root_window(), 0, 0);
    other.set_events();
    other.map();
    send(other, FocusIn);
    other.set_text("p\xC5\x99\xC3\xADlis");
    type_key(other, XK_c, ControlMask);

    type_key(entry, XK_v, ControlMask);
    settle();

    EXPECT_EQ(entry.get_text(), "p\xC5\x99\xC3\xADlis");
    EXPECT_EQ(changed.calls, 1);
}

TEST_F(EntryEditTest, shift_insert_pastes_the_clipboard)
{
    entry.set_text("a");
    type_key(entry, XK_c, ControlMask); // our own text
    type_key(entry, XK_Insert, ShiftMask);

    EXPECT_EQ(entry.get_text(), "aa");
}

TEST_F(EntryEditTest, the_middle_button_pastes_the_primary_selection)
{
    const tiny::Display& d = tiny::get_display();
    const ::Window owner =
            XCreateSimpleWindow(d, root_window(), 0, 0, 5, 5, 0, 0, 0);
    tiny::Clipboard primary(d, owner);
    for (const int type : {SelectionNotify, SelectionRequest, SelectionClear}) {
        tiny::handlers.set_handler(
                static_cast<uint16_t>(type), owner,
                [&primary](const XEvent& e) { primary.handle(e); });
    }
    primary.own(tiny::Clipboard::Which::Primary, "line 1\nline 2");

    entry.set_text(">");
    send(entry.get_window(), button(entry, Button2));
    settle();

    EXPECT_EQ(entry.get_text(), ">line 1 line 2");
    for (const int type : {SelectionNotify, SelectionRequest, SelectionClear}) {
        tiny::handlers.unset_handler(static_cast<uint16_t>(type), owner);
    }
}

TEST_F(EntryEditTest, paste_with_nothing_to_paste_changes_nothing)
{
    XSetSelectionOwner(tiny::get_display(), tiny::get_display().CLIPBOARD, None,
                       CurrentTime);
    entry.set_text("a");
    type_key(entry, XK_v, ControlMask);
    settle();

    EXPECT_EQ(entry.get_text(), "a");
    EXPECT_EQ(changed.calls, 0);
}
