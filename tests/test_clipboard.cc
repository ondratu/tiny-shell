#include "helpers.h"

#include <X11/Xatom.h>

#include "clipboard.h"

namespace {

// A window with a Clipboard; its events go to the handlers like in the loop.
class Holder {
  public:
    Holder():
        window(XCreateSimpleWindow(tiny::get_display(), root_window(), 0, 0, 10,
                                   10, 0, 0, 0)),
        clipboard(tiny::get_display(), window)
    {
        for (const int type :
             {SelectionNotify, SelectionRequest, SelectionClear}) {
            tiny::handlers.set_handler(
                    static_cast<uint16_t>(type), window,
                    [this](const XEvent& e) { clipboard.handle(e); });
        }
    }

    ~Holder()
    {
        for (const int type :
             {SelectionNotify, SelectionRequest, SelectionClear}) {
            tiny::handlers.unset_handler(static_cast<uint16_t>(type), window);
        }
    }

    // Asks for a selection and lets the events run until the answer
    std::string paste(tiny::Clipboard::Which which)
    {
        std::string got;
        clipboard.request(which, [&got](const std::string& t) { got = t; });
        for (int i = 0; i < 5; ++i) {
            pump_events();
        }
        return got;
    }

    ::Window window;
    tiny::Clipboard clipboard;
};

} // namespace

TEST(SingleLineTest, line_breaks_and_tabs_are_spaces)
{
    EXPECT_EQ(tiny::single_line("a\nb\r\nc\td"), "a b  c d");
}

TEST(SingleLineTest, other_control_characters_are_dropped)
{
    EXPECT_EQ(tiny::single_line(std::string("a\x01"
                                            "b\x7F"
                                            "c\0d",
                                            7)),
              "abcd");
}

TEST(SingleLineTest, utf8_is_kept)
{
    EXPECT_EQ(tiny::single_line("\xC4\x9B\xC5\xA1"), "\xC4\x9B\xC5\xA1");
}

TEST(ClipboardTest, a_text_goes_from_one_window_to_another)
{
    Holder owner;
    Holder reader;
    owner.clipboard.own(tiny::Clipboard::Which::Clipboard,
                        "p\xC5\x99\xC3\xADlis");

    EXPECT_EQ(reader.paste(tiny::Clipboard::Which::Clipboard),
              "p\xC5\x99\xC3\xADlis");
}

TEST(ClipboardTest, primary_and_clipboard_are_separate)
{
    Holder owner;
    Holder reader;
    owner.clipboard.own(tiny::Clipboard::Which::Primary, "selected");
    owner.clipboard.own(tiny::Clipboard::Which::Clipboard, "copied");

    EXPECT_EQ(reader.paste(tiny::Clipboard::Which::Primary), "selected");
    EXPECT_EQ(reader.paste(tiny::Clipboard::Which::Clipboard), "copied");
}

TEST(ClipboardTest, the_owner_pastes_its_own_text_at_once)
{
    Holder owner;
    owner.clipboard.own(tiny::Clipboard::Which::Clipboard, "mine");
    std::string got;

    owner.clipboard.request(tiny::Clipboard::Which::Clipboard,
                            [&got](const std::string& t) { got = t; });

    EXPECT_EQ(got, "mine");
}

TEST(ClipboardTest, nothing_comes_when_the_owner_has_no_such_text)
{
    Holder reader;
    std::string got = "untouched";
    XSetSelectionOwner(tiny::get_display(), tiny::get_display().CLIPBOARD, None,
                       CurrentTime);

    reader.clipboard.request(tiny::Clipboard::Which::Clipboard,
                             [&got](const std::string& t) { got = t; });
    for (int i = 0; i < 3; ++i) {
        pump_events();
    }

    EXPECT_EQ(got, "untouched");
}

// The server tells the old owner only when another program takes the
// selection, so the other program is another connection.
TEST(ClipboardTest, a_program_that_takes_the_selection_clears_ours)
{
    Holder holder;
    holder.clipboard.own(tiny::Clipboard::Which::Primary, "one");
    ASSERT_TRUE(holder.clipboard.owns(tiny::Clipboard::Which::Primary));

    ::Display* other = XOpenDisplay(nullptr);
    ASSERT_NE(other, nullptr);
    const ::Window w = XCreateSimpleWindow(other, DefaultRootWindow(other), 0,
                                           0, 5, 5, 0, 0, 0);
    XSetSelectionOwner(other, XA_PRIMARY, w, CurrentTime);
    XSync(other, False);
    for (int i = 0; i < 3; ++i) {
        pump_events();
    }
    XCloseDisplay(other);

    EXPECT_FALSE(holder.clipboard.owns(tiny::Clipboard::Which::Primary));
    EXPECT_TRUE(
            holder.clipboard.owned(tiny::Clipboard::Which::Primary).empty());
}

TEST(ClipboardTest, a_selection_that_is_not_ours_is_ignored)
{
    Holder holder;
    holder.clipboard.own(tiny::Clipboard::Which::Primary, "one");
    XEvent e = make_event(SelectionClear);
    e.xselectionclear.window = holder.window;
    e.xselectionclear.selection = XA_SECONDARY;

    EXPECT_TRUE(holder.clipboard.handle(e));
    EXPECT_TRUE(holder.clipboard.owns(tiny::Clipboard::Which::Primary));
}

TEST(ClipboardTest, the_old_target_string_gets_latin1)
{
    Holder owner;
    owner.clipboard.own(tiny::Clipboard::Which::Clipboard,
                        "caf\xC3\xA9 \xC4\x9B");
    const Atom property = tiny::get_display().TINY_SELECTION;
    XConvertSelection(tiny::get_display(), tiny::get_display().CLIPBOARD,
                      XA_STRING, property, owner.window, CurrentTime);
    for (int i = 0; i < 5; ++i) {
        pump_events();
    }

    Atom type;
    int format;
    unsigned long count;
    unsigned long after;
    unsigned char* data = nullptr;
    XGetWindowProperty(tiny::get_display(), owner.window, property, 0, 100,
                       True, AnyPropertyType, &type, &format, &count, &after,
                       &data);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(data), count), "caf\xE9 ?");
    XFree(data);
}

TEST(ClipboardTest, targets_lists_the_text_formats)
{
    Holder owner;
    owner.clipboard.own(tiny::Clipboard::Which::Clipboard, "x");
    const Atom property = tiny::get_display().TINY_SELECTION;
    XConvertSelection(tiny::get_display(), tiny::get_display().CLIPBOARD,
                      tiny::get_display().TARGETS, property, owner.window,
                      CurrentTime);
    for (int i = 0; i < 5; ++i) {
        pump_events();
    }

    Atom type;
    int format;
    unsigned long count;
    unsigned long after;
    unsigned char* data = nullptr;
    XGetWindowProperty(tiny::get_display(), owner.window, property, 0, 100,
                       True, AnyPropertyType, &type, &format, &count, &after,
                       &data);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(type, static_cast<Atom>(XA_ATOM));
    EXPECT_EQ(count, 3u);
    XFree(data);
}

TEST(ClipboardTest, an_unknown_target_is_refused)
{
    Holder owner;
    owner.clipboard.own(tiny::Clipboard::Which::Clipboard, "x");
    const Atom property = tiny::get_display().TINY_SELECTION;
    XConvertSelection(tiny::get_display(), tiny::get_display().CLIPBOARD,
                      XA_PIXMAP, property, owner.window, CurrentTime);
    for (int i = 0; i < 5; ++i) {
        pump_events();
    }

    Atom type;
    int format;
    unsigned long count;
    unsigned long after;
    unsigned char* data = nullptr;
    XGetWindowProperty(tiny::get_display(), owner.window, property, 0, 100,
                       True, AnyPropertyType, &type, &format, &count, &after,
                       &data);
    EXPECT_EQ(data, nullptr);
}

TEST(ClipboardTest, other_events_are_not_handled)
{
    Holder holder;

    EXPECT_FALSE(holder.clipboard.handle(make_event(KeyPress)));
}

TEST(ClipboardTest, the_selection_is_given_back_on_destruction)
{
    {
        Holder owner;
        owner.clipboard.own(tiny::Clipboard::Which::Primary, "gone");
        ASSERT_EQ(XGetSelectionOwner(tiny::get_display(), XA_PRIMARY),
                  owner.window);
    }
    EXPECT_EQ(XGetSelectionOwner(tiny::get_display(), XA_PRIMARY), None);
}
