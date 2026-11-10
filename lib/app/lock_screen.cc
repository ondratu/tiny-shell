#include "app/lock_screen.h"

#include <X11/Xutil.h>
#include <poll.h>

#include <algorithm>
#include <cerrno>
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

constexpr int spacing = 10;
constexpr int line_height = 20;
constexpr int entry_height = 28;
constexpr int dialog_width = 440;
//! A failure that is not a wrong password waits, so the tries don't spin
constexpr std::chrono::milliseconds failure_delay(1000);

} // namespace

class LockScreen::Dialog : public tiny::Box {
  public:
    explicit Dialog(const std::string& user):
        tiny::Box(tiny::Box::Type::Vertical, width(), height()),
        who(inner_width(), line_height, user),
        question(inner_width(), line_height, ""),
        entry(inner_width() - 2 * tiny::theme.entry.border_width, entry_height),
        status(inner_width(), line_height, "")
    {}

    void build(::Window parent, int x, int y)
    {
        realize(parent, x, y);
        push_start(&who, spacing, spacing);
        push_start(&question, spacing, spacing / 2);
        push_start(&entry, spacing, spacing / 2);
        push_start(&status, spacing, spacing / 2);
    }

    static int width() { return dialog_width * 1; }

    static int height()
    {
        return spacing + 3 * line_height + 3 * (spacing / 2) + entry_height +
               2 * tiny::theme.entry.border_width + spacing;
    }

    tiny::Label who;
    tiny::Label question;
    tiny::Entry entry;
    tiny::Label status;

  private:
    static int inner_width() { return dialog_width - 2 * spacing; }
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
    dialog->build(cover, 0, 0);
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
    dialog->map_all();
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
    const int x = area.x + (area.width - Dialog::width()) / 2;
    const int y = area.y + (area.height - Dialog::height()) / 2;
    XMoveWindow(display, dialog->get_window(), x, y);
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
    asking = false;
    if (!session.answer(text)) {
        set_status("The check is not running");
    }
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
