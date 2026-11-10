#include "app/lock_screen.h"

#include <X11/Xutil.h>
#include <poll.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <ctime>
#include <thread>
#include <utility>

#include "object.h"
#include "theme.h"
#include "wm/monitors.h"

namespace tiny {
extern Handlers handlers;
}

namespace tiny::app {

namespace {

//! The sizes at the scale 1 (`px()` makes them as big as the theme)
constexpr int base_spacing = 14;
constexpr int base_width = 440;
constexpr int base_entry_height = 28;
//! The name of the user is this much bigger than the text, the time more
constexpr double name_scale = 1.6;
constexpr double clock_scale = 4.5;
//! A failure that is not a wrong password waits, so the tries don't spin
constexpr std::chrono::milliseconds failure_delay(1000);

int px(int value)
{
    return static_cast<int>(std::lround(value * tiny::theme.scale_factor));
}

//! The height of one line of the label at this scale of its font
int line_of(double font_scale)
{
    tiny::Label probe(1, 1);
    probe.set_font_scale(font_scale);
    return probe.natural_height();
}

int entry_height()
{
    const XftFont* font = tiny::theme.entry.get_font();
    return std::max(px(base_entry_height),
                    font->ascent + font->descent +
                            2 * (tiny::theme.entry.padding_width + 4));
}

} // namespace

/**
 * The dialog: the name of the user, what is asked, the entry and what is said
 * about the last try, one under another with a space between them, all in the
 * middle. Over it is the time in big letters.
 */
class LockScreen::Dialog : public tiny::Box {
  public:
    explicit Dialog(const std::string& user):
        tiny::Box(tiny::Box::Type::Vertical, width(), height()),
        who(inner_width(), line_of(name_scale), user),
        question(inner_width(), line_of(1), ""),
        entry(inner_width() - 2 * tiny::theme.entry.border_width,
              entry_height()),
        status(inner_width(), line_of(1), ""),
        clock(width(), line_of(clock_scale), "")
    {
        who.set_font_scale(name_scale);
        who.set_align(tiny::Label::Align::Center);
        question.set_align(tiny::Label::Align::Center);
        status.set_align(tiny::Label::Align::Center);
        clock.set_font_scale(clock_scale);
        clock.set_align(tiny::Label::Align::Center);
    }

    void build(::Window parent_window)
    {
        realize(parent_window, 0, 0);
        const int gap = px(base_spacing);
        push_start(&who, gap, gap);
        push_start(&question, gap, gap / 2);
        push_start(&entry, gap, gap / 2);
        push_start(&status, gap, gap / 2);
        clock.realize(parent_window, 0, 0);
    }

    static int width() { return px(base_width); }

    static int height()
    {
        const int gap = px(base_spacing);
        return gap + line_of(name_scale) + gap + line_of(1) + gap / 2 +
               entry_height() + 2 * tiny::theme.entry.border_width + gap / 2 +
               line_of(1) + gap;
    }

    //! The time over the dialog and the space between them
    static int clock_height() { return line_of(clock_scale); }
    static int clock_gap() { return px(base_spacing) * 2; }

    tiny::Label who;
    tiny::Label question;
    tiny::Entry entry;
    tiny::Label status;
    tiny::Label clock;

  private:
    static int inner_width() { return width() - 2 * px(base_spacing); }
};

LockScreen::LockScreen(tiny::Display& display_arg,
                       std::vector<std::string> command, std::string user_arg):
    display(display_arg), session(std::move(command)), user(std::move(user_arg))
{
    ::Display* const xdisplay = display;
    const int screen = XDefaultScreen(xdisplay);
    XSetWindowAttributes attributes = {};
    attributes.override_redirect = True; // no window manager has a say
    attributes.background_pixel = BlackPixel(xdisplay, screen);
    attributes.event_mask = VisibilityChangeMask;
    cover = XCreateWindow(
            display, XDefaultRootWindow(display), 0, 0,
            static_cast<unsigned>(DisplayWidth(xdisplay, screen)),
            static_cast<unsigned>(DisplayHeight(xdisplay, screen)), 0,
            CopyFromParent, InputOutput, CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWEventMask, &attributes);
    // no pointer to see on a locked screen
    const char empty = 0;
    const Pixmap bitmap = XCreateBitmapFromData(display, cover, &empty, 1, 1);
    XColor black = {};
    blank_cursor =
            XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0);
    XFreePixmap(display, bitmap);
    XDefineCursor(display, cover, blank_cursor);

    dialog = std::make_unique<Dialog>(user);
    dialog->build(cover);
    // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall): nothing derives from Dialog
    dialog->set_events();
    dialog->entry.set_password();
    dialog->entry.on_activate.connect(
            [this](tiny::Object*, const XEvent&, void*) { submit(); });
    dialog->entry.on_cancel.connect([this](tiny::Object*, const XEvent&,
                                           void*) {
        dialog->entry.clear(); // Escape forgets what was typed, nothing more
    });
    // what was said about the last try is kept until the user types again
    dialog->entry.on_change.connect(
            [this](tiny::Object*, const XEvent&, void*) {
                if (!dialog->entry.get_text().empty() && !status_text.empty()) {
                    set_status("");
                }
            });
    place_dialog();

    tiny::handlers.set_handler(
            VisibilityNotify, cover,
            [this](const XEvent& event) { on_visibility(event); });
    XMapWindow(display, cover);
    dialog->clock.set_events();
    dialog->map_all();
    dialog->clock.map();
    update_clock(std::time(nullptr));
    XRaiseWindow(display, cover);
    XFlush(display);
}

LockScreen::~LockScreen()
{
    tiny::handlers.unset_handler(VisibilityNotify, cover);
    if (grabbed) {
        XUngrabKeyboard(display, CurrentTime);
        XUngrabPointer(display, CurrentTime);
    }
    dialog.reset(); // its windows are children of the cover
    XDestroyWindow(display, cover);
    if (blank_cursor != None) {
        XFreeCursor(display, blank_cursor);
    }
    XFlush(display);
}

void LockScreen::update_clock(std::time_t now)
{
    char text[16];
    tm local = {};
    localtime_r(&now, &local);
    if (strftime(text, sizeof text, "%H:%M", &local) == 0) {
        text[0] = '\0';
    }
    clock_text_ = text;
    dialog->clock.set_text(clock_text_);
    // the next change is at the start of the next minute
    const int left = 60 - local.tm_sec;
    next_clock = Clock::now() + std::chrono::seconds(left);
}

Entry& LockScreen::entry()
{
    return dialog->entry;
}

void LockScreen::place_dialog()
{
    // in the middle of the first monitor (the primary one if RandR knows it)
    const auto monitors =
            tiny::wm::read_monitors(display, XDefaultRootWindow(display));
    ::Display* const xdisplay = display;
    const int screen = XDefaultScreen(xdisplay);
    tiny::Rect area = {0, 0, DisplayWidth(xdisplay, screen),
                       DisplayHeight(xdisplay, screen)};
    for (const auto& monitor : monitors) {
        if (monitor.primary || &monitor == &monitors.front()) {
            area = monitor.area;
            if (monitor.primary) {
                break;
            }
        }
    }
    // the time and the dialog are one block in the middle of the monitor
    const int block =
            Dialog::clock_height() + Dialog::clock_gap() + Dialog::height();
    const int x = area.x + (area.width - Dialog::width()) / 2;
    const int y = area.y + (area.height - block) / 2;
    XMoveWindow(display, dialog->clock.get_window(), x, y);
    XMoveWindow(display, dialog->get_window(), x,
                y + Dialog::clock_height() + Dialog::clock_gap());
}

bool LockScreen::lock(std::chrono::milliseconds patience)
{
    const auto until = Clock::now() + patience;
    // The keyboard goes to the window of the entry, whatever has the focus,
    // so the typing can't reach another client.
    const ::Window keys = dialog->entry.get_window();
    for (;;) {
        XSync(display, False);
        const int keyboard = XGrabKeyboard(display, keys, False, GrabModeAsync,
                                           GrabModeAsync, CurrentTime);
        if (keyboard == GrabSuccess) {
            const int pointer = XGrabPointer(
                    display, cover, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, cover, blank_cursor,
                    CurrentTime);
            if (pointer == GrabSuccess) {
                grabbed = true;
                give_focus();
                start_try();
                return true;
            }
            XUngrabKeyboard(display, CurrentTime);
        }
        if (Clock::now() >= until) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void LockScreen::give_focus()
{
    XWindowAttributes attributes;
    if (XGetWindowAttributes(display, dialog->entry.get_window(),
                             &attributes) != 0 &&
        attributes.map_state == IsViewable) {
        dialog->entry.set_focus();
        focused = true;
    }
}

void LockScreen::on_visibility(const XEvent& event)
{
    // another window over ours (a menu, a notification): back to the top
    if (event.xvisibility.state != VisibilityUnobscured && !done) {
        XRaiseWindow(display, cover);
    }
}

void LockScreen::set_status(const std::string& text)
{
    status_text = text;
    dialog->status.set_text(text);
}

void LockScreen::set_prompt(const std::string& text, bool hidden)
{
    prompt_text = text;
    dialog->question.set_text(text);
    dialog->entry.clear();
    dialog->entry.set_password(hidden);
    asking = true;
}

void LockScreen::start_try()
{
    asking = false;
    retry_at = Clock::time_point();
    if (!session.start()) {
        set_status("Can't check the password: " + session.error());
        retry_at = Clock::now() + failure_delay;
    }
}

void LockScreen::submit()
{
    if (!asking) {
        return; // nothing asks; what was typed stays
    }
    std::string text = dialog->entry.get_text();
    if (text.find_first_of("\r\n") != std::string::npos) {
        // the protocol has one line for an answer; a changed password would
        // only be "wrong" without a reason
        set_status("The password can't have a line break");
        wipe(text);
        dialog->entry.clear();
        return;
    }
    asking = false;
    // a check can take its time (a wrong password waits)
    set_status(session.answer(text) ? "Checking..."
                                    : "The check is not running");
    wipe(text);
    dialog->entry.clear();
}

bool LockScreen::handle_messages(const std::vector<Message>& messages)
{
    for (const Message& message : messages) {
        switch (message.kind) {
            case Kind::PromptHidden:
                set_prompt(message.text, true);
                break;
            case Kind::PromptVisible:
                set_prompt(message.text, false);
                break;
            case Kind::Info:
            case Kind::Error:
                set_status(message.text);
                break;
            case Kind::Result:
                handle_result(outcome_of(message.text));
                return true; // the program ends after the result
            default:
                break; // not for us
        }
    }
    return false;
}

void LockScreen::handle_result(const Outcome& outcome)
{
    session.stop();
    asking = false;
    switch (outcome.verdict) {
        case Verdict::Right:
            unlock();
            return;
        case Verdict::Wrong:
            set_status("Wrong password");
            dialog->entry.clear();
            start_try(); // the check has made its delay already
            return;
        default:
            set_status("Can't check the password" +
                       (outcome.text.empty() ? "" : ": " + outcome.text));
            dialog->entry.clear();
            retry_at = Clock::now() + failure_delay;
            return;
    }
}

void LockScreen::unlock()
{
    done = true;
    if (grabbed) {
        XUngrabKeyboard(display, CurrentTime);
        XUngrabPointer(display, CurrentTime);
        grabbed = false;
    }
    dialog->entry.clear();
    XUnmapWindow(display, cover);
    XFlush(display);
}

void LockScreen::handle_events()
{
    while (XPending(display) > 0) {
        XEvent event;
        XNextEvent(display, &event);
        const auto id = std::make_pair(event.type, event.xany.window);
        if (tiny::handlers.count(id)) {
            tiny::handlers.call_handler(id, event);
        }
    }
}

bool LockScreen::step(int timeout_ms)
{
    handle_events();
    if (done) {
        return false;
    }
    if (!focused && grabbed) {
        give_focus();
    }
    // A request that waits for a reply reads the events that came meanwhile
    // into the queue of Xlib, and poll() sees only the socket.
    if (XPending(display) > 0) {
        return true;
    }
    int wait = timeout_ms;
    if (Clock::now() >= next_clock) {
        update_clock(std::time(nullptr));
    }
    {
        const auto until_clock =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        next_clock - Clock::now())
                        .count();
        const int ms = static_cast<int>(std::max<long long>(until_clock, 0));
        wait = wait < 0 ? ms : std::min(wait, ms);
    }
    if (retry_at != Clock::time_point()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  retry_at - Clock::now())
                                  .count();
        const int until_retry = static_cast<int>(std::max<long long>(left, 0));
        wait = wait < 0 ? until_retry : std::min(wait, until_retry);
    }
    pollfd fds[2] = {{XConnectionNumber(display), POLLIN, 0},
                     {session.descriptor(), POLLIN, 0}};
    const int count = session.running() ? 2 : 1;
    if (poll(fds, static_cast<nfds_t>(count), wait) < 0 && errno != EINTR) {
        return !done;
    }
    if (session.running() && fds[1].revents != 0) {
        const bool result = handle_messages(session.read());
        if (!result && !session.running() && !done) {
            // the program ended without a result: that is not an unlock
            asking = false;
            set_status("The check ended without an answer");
            dialog->entry.clear();
            retry_at = Clock::now() + failure_delay;
        }
    }
    if (retry_at != Clock::time_point() && Clock::now() >= retry_at && !done) {
        start_try();
    }
    handle_events();
    return !done;
}

void LockScreen::run()
{
    while (step(-1)) {}
}

} // namespace tiny::app
