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
#include "wm/clock_label.h"
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
constexpr double date_scale = 1.8;
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
        clock(wide(), line_of(clock_scale), ""),
        date(wide(), line_of(date_scale), ""),
        hint(wide(), line_of(1), "Click or press a key to unlock")
    {
        who.set_font_scale(name_scale);
        who.set_align(tiny::Label::Align::Center);
        question.set_align(tiny::Label::Align::Center);
        status.set_align(tiny::Label::Align::Center);
        clock.set_font_scale(clock_scale);
        clock.set_align(tiny::Label::Align::Center);
        date.set_font_scale(date_scale);
        date.set_align(tiny::Label::Align::Center);
        hint.set_align(tiny::Label::Align::Center);
    }

    //! No frame: the text is on the black screen
    int get_border() const override { return 0; }

    void build(::Window parent_window)
    {
        realize(parent_window, 0, 0);
        const int gap = px(base_spacing);
        push_start(&who, gap, gap);
        push_start(&question, gap, gap / 2);
        push_start(&entry, gap, gap / 2);
        push_start(&status, gap, gap / 2);
        for (tiny::Label* label : {&clock, &date, &hint}) {
            label->realize(parent_window, 0, 0);
            label->set_events();
        }
    }

    static int width() { return px(base_width); }

    //! The time, the date and the hint are wider than the dialog
    static int wide() { return width() * 3 / 2; }

    static int height()
    {
        const int gap = px(base_spacing);
        return gap + line_of(name_scale) + gap + line_of(1) + gap / 2 +
               entry_height() + 2 * tiny::theme.entry.border_width + gap / 2 +
               line_of(1) + gap;
    }

    //! The time and the date over the dialog, and the space between them
    static int clock_height() { return line_of(clock_scale); }
    static int date_height() { return line_of(date_scale); }
    static int clock_gap() { return px(base_spacing) * 2; }

    tiny::Label who;
    tiny::Label question;
    tiny::Entry entry;
    tiny::Label status;
    tiny::Label clock;
    tiny::Label date;
    tiny::Label hint; //!< in the place of the dialog when it is not open

  private:
    static int inner_width() { return width() - 2 * px(base_spacing); }
};

std::string date_format_of(const std::string& clock_format)
{
    // the directives of the time of the day
    const std::string time_of_day = "HIklMSpPRTrXZzs";
    std::string date;
    for (size_t i = 0; i < clock_format.size(); ++i) {
        if (clock_format[i] != '%' || i + 1 >= clock_format.size()) {
            date += clock_format[i];
            continue;
        }
        size_t end = i + 1;
        while (end < clock_format.size() &&
               std::string("-_0^#EO").find(clock_format[end]) !=
                       std::string::npos) {
            ++end; // flags and modifiers of the directive
        }
        if (end >= clock_format.size()) {
            date += clock_format.substr(i);
            break;
        }
        if (time_of_day.find(clock_format[end]) == std::string::npos) {
            date += clock_format.substr(i, end - i + 1);
        } else {
            date += '\x01'; // a mark of the place where the time was
        }
        i = end;
    }
    // the marks and what is around them (blanks, separators) are dropped from
    // the ends; a mark in the middle takes its separators with it
    const std::string separators = " \t,;:-/\x01";
    const size_t first = date.find_first_not_of(separators);
    if (first == std::string::npos) {
        return LockOptions().date_format;
    }
    date = date.substr(first, date.find_last_not_of(separators) - first + 1);
    std::string clean;
    for (const char c : date) {
        if (c != '\x01') {
            clean += c;
        }
    }
    return clean;
}

LockScreen::LockScreen(tiny::Display& display_arg,
                       std::vector<std::string> command, std::string user_arg,
                       LockOptions options_arg):
    display(display_arg),
    session(std::move(command)),
    user(std::move(user_arg)),
    options(std::move(options_arg))
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
    dialog->entry.on_cancel.connect(
            [this](tiny::Object*, const XEvent&, void*) {
                hide_dialog(); // Escape: back to the time, nothing is typed
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
    dialog->map_all();
    for (tiny::Label* label : {&dialog->clock, &dialog->date, &dialog->hint}) {
        label->map();
    }
    update_clock(std::time(nullptr));
    set_state(State::Idle);
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
    // the same function as the clock of the panel: the same text
    std::string date;
    if (!options.date_format.empty()) {
        date = tiny::wm::ClockLabel::format_time(options.date_format, now);
    }
    date_text_ = date;
    dialog->date.set_text(date_text_);
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
    // the time, the date and the dialog are one block in the middle of the
    // monitor; the dialog and the hint take the same place
    const int block = Dialog::clock_height() + Dialog::date_height() +
                      Dialog::clock_gap() + Dialog::height();
    const int x = area.x + (area.width - Dialog::width()) / 2;
    const int wide_x = area.x + (area.width - Dialog::wide()) / 2;
    const int y = area.y + (area.height - block) / 2;
    const int dialog_y = y + Dialog::clock_height() + Dialog::date_height() +
                         Dialog::clock_gap();
    XMoveWindow(display, dialog->clock.get_window(), wide_x, y);
    XMoveWindow(display, dialog->date.get_window(), wide_x,
                y + Dialog::clock_height());
    XMoveWindow(display, dialog->get_window(), x, dialog_y);
    XMoveWindow(display, dialog->hint.get_window(), wide_x,
                dialog_y + (Dialog::height() - line_of(1)) / 2);
}

bool LockScreen::lock(std::chrono::milliseconds patience)
{
    const auto until = Clock::now() + patience;
    // The keyboard goes to the lock, whatever has the focus, so the typing
    // can't reach another client (the entry is not shown in Idle, so it can't
    // be the window of the grab; the keys that do not come to it are given to
    // it, see `handle_input()`).
    for (;;) {
        XSync(display, False);
        const int keyboard = XGrabKeyboard(display, cover, True, GrabModeAsync,
                                           GrabModeAsync, CurrentTime);
        if (keyboard == GrabSuccess) {
            const int pointer = XGrabPointer(
                    display, cover, False,
                    ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                    GrabModeAsync, GrabModeAsync, cover, blank_cursor,
                    CurrentTime);
            if (pointer == GrabSuccess) {
                grabbed = true;
                touch();
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
    if (state_ != State::Dialog) {
        return;
    }
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
        if (handle_input(event)) {
            continue;
        }
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
    if (state_ == State::Idle && blank_at != Clock::time_point()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  blank_at - Clock::now())
                                  .count();
        const int until_blank = static_cast<int>(std::max<long long>(left, 0));
        wait = wait < 0 ? until_blank : std::min(wait, until_blank);
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
    if (state_ == State::Idle && blank_at != Clock::time_point() &&
        Clock::now() >= blank_at) {
        blank();
    }
    handle_events();
    return !done;
}

void LockScreen::touch()
{
    blank_at = options.blank_after > std::chrono::milliseconds(0)
                       ? Clock::now() + options.blank_after
                       : Clock::time_point();
}

void LockScreen::set_state(State state)
{
    const State before = state_;
    state_ = state;
    if (before == State::Dialog && state != State::Dialog) {
        session.stop(); // nothing asks while nobody is there
        asking = false;
        retry_at = Clock::time_point();
        dialog->entry.clear();
        set_status("");
        dialog->question.set_text("");
        prompt_text.clear();
    }
    // what is seen in the state
    const bool seen = state != State::Black;
    const auto show = [seen](tiny::Widget& widget, bool wanted) {
        if (seen && wanted) {
            widget.map();
        } else {
            widget.unmap();
        }
    };
    show(dialog->clock, true);
    show(dialog->date, true);
    show(dialog->hint, state == State::Idle);
    show(*dialog, state == State::Dialog);
    if (state == State::Dialog) {
        give_focus();
    }
    if (state == State::Idle) {
        touch(); // the time to the black screen counts from here
    } else {
        blank_at = Clock::time_point(); // a dialog or a black screen stays
    }
    if (state == State::Dialog && before != State::Dialog) {
        start_try();
    }
    XFlush(display);
}

void LockScreen::show_dialog()
{
    if (state_ != State::Dialog) {
        set_state(State::Dialog);
    }
}

void LockScreen::hide_dialog()
{
    if (state_ == State::Dialog) {
        set_state(State::Idle);
    }
}

void LockScreen::blank()
{
    if (state_ == State::Idle) {
        set_state(State::Black);
    }
}

void LockScreen::wake()
{
    if (state_ == State::Black) {
        set_state(State::Idle);
    }
}

bool LockScreen::handle_input(XEvent& event)
{
    switch (event.type) {
        case KeyPress:
        case ButtonPress:
            if (state_ == State::Black) {
                wake();
                return true;
            }
            if (state_ == State::Idle) {
                show_dialog();
                return true;
            }
            if (event.type == KeyPress &&
                event.xkey.window != dialog->entry.get_window()) {
                // the key came to the window of the lock (something else has
                // the focus): it is for the entry all the same
                event.xkey.window = dialog->entry.get_window();
                event.xkey.subwindow = None;
            }
            return event.type == ButtonPress;
        case KeyRelease:
            if (state_ != State::Dialog) {
                return true;
            }
            event.xkey.window = dialog->entry.get_window();
            event.xkey.subwindow = None;
            return false;
        case ButtonRelease:
            return true;
        case MotionNotify:
            if (state_ == State::Black) {
                wake();
            } else if (state_ == State::Idle) {
                touch();
            }
            return true;
        default:
            return false;
    }
}

void LockScreen::run()
{
    while (step(-1)) {}
}

} // namespace tiny::app
