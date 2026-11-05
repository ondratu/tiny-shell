#include "helpers.h"

#include <X11/Xatom.h>

#include "xft_window.h"

namespace {

// Counts what the base calls and paints a white window
class TestWindow : public tiny::XftWindow {
  public:
    TestWindow(bool input_method = false): XftWindow(tiny::get_display())
    {
        create({"Test", "test", "Test", 120, 80, 40, 30, 0, input_method});
        black = &alloc_gray(0);
        white = &alloc_gray(255);
    }

    using XftWindow::alloc_color;
    using XftWindow::alloc_gray;
    using XftWindow::cached_fonts;
    using XftWindow::clip;
    using XftWindow::clipboard;
    using XftWindow::close_fonts;
    using XftWindow::fit;
    using XftWindow::font;
    using XftWindow::font_by_name;
    using XftWindow::frame;
    using XftWindow::text_in;
    using XftWindow::text_width;
    using XftWindow::unclip;
    using XftWindow::update;
    using XftWindow::wrap;

  protected:
    void paint() override
    {
        ++painted;
        fill(*white, {0, 0, width, height});
        fill(*black, {10, 10, 20, 20});
    }

    void on_resize(int w, int h) override { resized_to = {0, 0, w, h}; }

    void on_key(XKeyEvent& e) override
    {
        KeySym symbol = NoSymbol;
        typed = lookup(e, symbol);
        key = symbol;
    }

    void on_button_press(const XButtonEvent& e) override { pressed = e.button; }
    void on_button_release(const XButtonEvent& e) override
    {
        released = e.button;
    }

  public:
    XftColor* black = nullptr;
    XftColor* white = nullptr;
    int painted = 0;
    tiny::Rect resized_to{};
    KeySym key = NoSymbol;
    std::string typed;
    unsigned pressed = 0;
    unsigned released = 0;
};

// Like an event from the server: XFilterEvent needs its display
XEvent server_event(int type)
{
    XEvent e = make_event(type);
    e.xany.display = tiny::get_display();
    return e;
}

XEvent client_message(::Window window, Atom type, long data)
{
    XEvent e = server_event(ClientMessage);
    e.xclient.window = window;
    e.xclient.message_type = type;
    e.xclient.format = 32;
    e.xclient.data.l[0] = data;
    return e;
}

} // namespace

TEST(XftWindowTest, the_window_has_the_size_and_the_hints_of_the_spec)
{
    const TestWindow w;
    ::Display* display = tiny::get_display();

    XWindowAttributes a;
    XGetWindowAttributes(display, w.get_window(), &a);
    EXPECT_EQ(a.width, 120);
    EXPECT_EQ(a.height, 80);
    EXPECT_EQ(w.get_width(), 120);

    XClassHint hint = {};
    ASSERT_NE(XGetClassHint(display, w.get_window(), &hint), 0);
    EXPECT_STREQ(hint.res_name, "test");
    EXPECT_STREQ(hint.res_class, "Test");
    XFree(hint.res_name);
    XFree(hint.res_class);

    XSizeHints sizes;
    long supplied = 0;
    ASSERT_NE(XGetWMNormalHints(display, w.get_window(), &sizes, &supplied), 0);
    EXPECT_EQ(sizes.min_width, 40);
    EXPECT_EQ(sizes.min_height, 30);
}

TEST(XftWindowTest, the_icon_is_published_as_not_premultiplied_argb)
{
    TestWindow w;
    tiny::image::Image icon;
    icon.width = 2;
    icon.height = 1;
    // opaque red, and half transparent white (premultiplied: 0x80 each)
    icon.pixels = {0xFFFF0000, 0x80808080};

    w.set_icon({icon, tiny::image::Image{}}); // an empty one is skipped

    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long left = 0;
    unsigned char* data = nullptr;
    ASSERT_EQ(XGetWindowProperty(
                      tiny::get_display(), w.get_window(),
                      XInternAtom(tiny::get_display(), "_NET_WM_ICON", False),
                      0, 100, False, XA_CARDINAL, &type, &format, &count, &left,
                      &data),
              Success);
    ASSERT_EQ(format, 32);
    ASSERT_EQ(count, 4u);
    const auto* words = reinterpret_cast<const unsigned long*>(data);
    EXPECT_EQ(words[0], 2u);
    EXPECT_EQ(words[1], 1u);
    EXPECT_EQ(words[2] & 0xFFFFFFFFu, 0xFFFF0000u);
    EXPECT_EQ(words[3] & 0xFFFFFFFFu, 0x80FFFFFFu);
    XFree(data);
}

TEST(XftWindowTest, an_icon_with_no_picture_publishes_nothing)
{
    TestWindow w;

    w.set_icon({});

    int format = 0;
    Atom type = None;
    unsigned long count = 0;
    unsigned long left = 0;
    unsigned char* data = nullptr;
    XGetWindowProperty(tiny::get_display(), w.get_window(),
                       XInternAtom(tiny::get_display(), "_NET_WM_ICON", False),
                       0, 100, False, XA_CARDINAL, &type, &format, &count,
                       &left, &data);
    EXPECT_EQ(type, static_cast<Atom>(None));
    XFree(data);
}

TEST(XftWindowTest, update_paints_and_shows_the_buffer_in_the_window)
{
    TestWindow w;
    XMapWindow(tiny::get_display(), w.get_window());
    w.update();
    XSync(tiny::get_display(), False);

    EXPECT_EQ(w.painted, 1);
    const Image image(w.get_window(), 120, 80);
    EXPECT_EQ(image.at(15, 15), 0u);
    EXPECT_EQ(image.at(60, 60), 0xFFFFFFu);
}

TEST(XftWindowTest, a_new_size_makes_the_buffer_again_and_paints)
{
    TestWindow w;
    XEvent e = server_event(ConfigureNotify);
    e.xconfigure.width = 200;
    e.xconfigure.height = 100;

    w.dispatch(e);

    EXPECT_EQ(w.get_width(), 200);
    EXPECT_EQ(w.get_height(), 100);
    EXPECT_EQ(w.resized_to.width, 200);
    EXPECT_EQ(w.painted, 1);

    w.dispatch(e); // the same size: only moved
    EXPECT_EQ(w.painted, 1);
}

TEST(XftWindowTest, the_last_expose_of_a_series_shows_the_buffer)
{
    TestWindow w;
    XEvent e = server_event(Expose);
    e.xexpose.count = 1;
    w.dispatch(e);
    e.xexpose.count = 0;
    w.dispatch(e);
    SUCCEED();
}

TEST(XftWindowTest, keys_and_buttons_go_to_the_handlers)
{
    TestWindow w;
    XEvent key = key_event(w.get_window(), XK_Down);
    w.dispatch(key);
    EXPECT_EQ(w.key, static_cast<KeySym>(XK_Down));
    EXPECT_TRUE(w.typed.empty());

    XEvent button = server_event(ButtonPress);
    button.xbutton.button = Button3;
    w.dispatch(button);
    EXPECT_EQ(w.pressed, 3u);
    button.type = ButtonRelease;
    button.xbutton.button = Button1;
    w.dispatch(button);
    EXPECT_EQ(w.released, 1u);
}

TEST(XftWindowTest, an_input_method_is_used_for_the_typing)
{
    TestWindow w(true);
    XEvent focus = server_event(FocusIn);
    w.dispatch(focus);
    focus.type = FocusOut;
    w.dispatch(focus);
    XEvent key = key_event(w.get_window(), XK_Left);
    w.dispatch(key);
    EXPECT_EQ(w.key, static_cast<KeySym>(XK_Left));
}

TEST(XftWindowTest, run_ends_when_the_window_manager_asks_to_close)
{
    TestWindow w;
    ::Display* display = tiny::get_display();
    XEvent e = client_message(
            w.get_window(), tiny::get_display().WM_PROTOCOLS,
            static_cast<long>(tiny::get_display().WM_DELETE_WINDOW));
    XSendEvent(display, w.get_window(), False, NoEventMask, &e);
    XSync(display, False);

    w.run();

    EXPECT_GE(w.painted, 1);
}

TEST(XftWindowTest, another_message_does_not_close_the_window)
{
    TestWindow w;
    XEvent e =
            client_message(w.get_window(), tiny::get_display().WM_PROTOCOLS, 0);
    w.dispatch(e);
    w.quit();
    w.run(); // ends at once: the loop only checks the flag
    SUCCEED();
}

TEST(XftWindowTest, colors_are_allocated_and_stay_valid)
{
    TestWindow w;
    const XftColor& first = w.alloc_color(255, 0, 0);
    for (int i = 0; i < 20; ++i) {
        w.alloc_gray(static_cast<unsigned>(i));
    }
    EXPECT_EQ(first.color.red, 0xffff);
    EXPECT_EQ(first.color.green, 0);
}

TEST(XftWindowTest, fonts_are_cached_by_family_and_size_and_closed_together)
{
    TestWindow w;
    const XftFont* a = w.font("sans", 12);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(w.font("sans", 12), a);
    EXPECT_NE(w.font("sans", 20), a);
    const XftFont* named = w.font_by_name("sans-10");
    ASSERT_NE(named, nullptr);
    EXPECT_EQ(w.font_by_name("sans-10"), named);
    EXPECT_EQ(w.cached_fonts(), 3u);

    w.close_fonts();
    EXPECT_EQ(w.cached_fonts(), 0u);
}

TEST(XftWindowTest, text_is_measured_and_cut_with_an_ellipsis)
{
    TestWindow w;
    XftFont* f = w.font("sans", 14);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(w.text_width(nullptr, "text"), 0);
    const int full = w.text_width(f, "a long text to cut");
    EXPECT_GT(full, 0);

    EXPECT_EQ(w.fit(f, "a long text to cut", full), "a long text to cut");
    const std::string cut = w.fit(f, "a long text to cut", full / 2);
    EXPECT_LE(w.text_width(f, cut), full / 2);
    EXPECT_NE(cut.find("\xE2\x80\xA6"), std::string::npos);
    EXPECT_EQ(w.fit(f, "a long text", 1), "\xE2\x80\xA6");
    EXPECT_EQ(w.fit(f, "\xC3\xA9\xC3\xA9", 1), "\xE2\x80\xA6");
}

TEST(XftWindowTest,
     text_is_clipped_to_its_rectangle_and_nothing_draws_without_font)
{
    TestWindow w;
    XftFont* f = w.font("sans", 14);
    ASSERT_NE(f, nullptr);

    w.text_in(nullptr, *w.black, {0, 0, 50, 20}, 0, "x"); // no font: nothing
    w.update();
    w.text_in(f, *w.black, {0, 30, 50, 20}, 2, "Hello world", 1);
    w.frame(*w.black, {60, 30, 50, 20}, 2);
    w.clip({0, 0, 5, 5});
    w.unclip();
    w.update();
    XSync(tiny::get_display(), False);
    SUCCEED();
}

TEST(XftWindowTest, a_font_of_size_zero_is_not_the_font_of_a_name)
{
    TestWindow w;

    const XftFont* named = w.font_by_name("sans-10");

    EXPECT_NE(w.font("sans-10", 0), named);
}

TEST(XftWindowWrapTest, a_text_that_fits_is_one_line)
{
    TestWindow w;
    XftFont* f = w.font("sans", 12);
    ASSERT_NE(f, nullptr);

    EXPECT_EQ(w.wrap(f, "one two", 10000),
              (std::vector<std::string>{"one two"}));
    EXPECT_EQ(w.wrap(f, "", 100), (std::vector<std::string>{""}));
}

TEST(XftWindowWrapTest, a_line_breaks_at_a_space)
{
    TestWindow w;
    XftFont* f = w.font("sans", 12);
    ASSERT_NE(f, nullptr);
    const int one = w.text_width(f, "alpha beta");

    const std::vector<std::string> lines =
            w.wrap(f, "alpha beta gamma delta", one);

    ASSERT_GE(lines.size(), 2u);
    for (const std::string& line : lines) {
        EXPECT_LE(w.text_width(f, line), one) << line;
    }
    std::string all;
    for (const std::string& line : lines) {
        all += (all.empty() ? "" : " ") + line;
    }
    EXPECT_EQ(all, "alpha beta gamma delta");
}

TEST(XftWindowWrapTest, a_line_break_of_the_text_starts_a_line)
{
    TestWindow w;
    XftFont* f = w.font("sans", 12);
    ASSERT_NE(f, nullptr);

    EXPECT_EQ(w.wrap(f, "a\nb", 10000), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(w.wrap(f, "a\n\nb", 10000),
              (std::vector<std::string>{"a", "", "b"}));
}

TEST(XftWindowWrapTest, a_word_wider_than_the_line_is_cut)
{
    TestWindow w;
    XftFont* f = w.font("sans", 12);
    ASSERT_NE(f, nullptr);
    const int width = w.text_width(f, "WWWW");

    const std::vector<std::string> lines = w.wrap(
            f, "WWWWWWWWWWWW \xC4\x9B\xC4\x9B\xC4\x9B\xC4\x9B\xC4\x9B", width);

    ASSERT_GT(lines.size(), 2u);
    for (const std::string& line : lines) {
        EXPECT_FALSE(line.empty());
        // never inside of a UTF-8 character
        EXPECT_NE(static_cast<unsigned char>(line.front()) & 0xC0, 0x80);
    }
}

TEST(XftWindowClipboardTest,
     the_window_has_the_selections_and_handles_their_events)
{
    TestWindow w;
    w.clipboard().own(tiny::Clipboard::Which::Primary, "text");
    std::string got;
    w.clipboard().request(tiny::Clipboard::Which::Primary,
                          [&got](const std::string& t) { got = t; });
    EXPECT_EQ(got, "text");

    XEvent clear = server_event(SelectionClear);
    clear.xselectionclear.window = w.get_window();
    clear.xselectionclear.selection = XA_PRIMARY;
    w.dispatch(clear);

    EXPECT_FALSE(w.clipboard().owns(tiny::Clipboard::Which::Primary));
}

TEST(XftWindowClipboardTest, a_request_for_the_text_is_answered_by_dispatch)
{
    TestWindow w;
    w.clipboard().own(tiny::Clipboard::Which::Clipboard, "copied");
    const ::Window asker = XCreateSimpleWindow(
            tiny::get_display(), root_window(), 0, 0, 5, 5, 0, 0, 0);
    XConvertSelection(tiny::get_display(), tiny::get_display().CLIPBOARD,
                      tiny::get_display().UTF8_STRING,
                      tiny::get_display().TINY_SELECTION, asker, CurrentTime);
    XSync(tiny::get_display(), False);

    XEvent e;
    while (XCheckTypedEvent(tiny::get_display(), SelectionRequest, &e)) {
        w.dispatch(e); // what the loop of the program does
    }
    XSync(tiny::get_display(), False);

    Atom type;
    int format;
    unsigned long count;
    unsigned long after;
    unsigned char* data = nullptr;
    XGetWindowProperty(tiny::get_display(), asker,
                       tiny::get_display().TINY_SELECTION, 0, 100, True,
                       AnyPropertyType, &type, &format, &count, &after, &data);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(std::string(reinterpret_cast<char*>(data), count), "copied");
    XFree(data);
    XDestroyWindow(tiny::get_display(), asker);
}
