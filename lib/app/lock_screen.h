#pragma once

#include <X11/Xlib.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "app/lock_session.h"
#include "container.h"
#include "entry.h"
#include "label.h"

namespace tiny::app {

/**
 * @short The window of the screen lock: it covers the screen, holds the
 * keyboard and the pointer, and asks for the password.
 *
 * It does not check anything itself: the questions and the answers go to
 * `tiny-lock-auth` (see `AuthSession`), and the screen is unlocked only by its
 * result `OK`. A try that goes wrong (a wrong password, no answer, the program
 * is gone) starts a new one, so the screen is never unlocked by an error.
 *
 * ```
 * LockScreen screen(display, {"tiny-lock-auth"}, "anna");
 * if (screen.lock()) {
 *     screen.run(); // returns when it is unlocked
 * }
 * ```
 */
class LockScreen {
  public:
    using Clock = std::chrono::steady_clock;

    /**
     * `command` starts `tiny-lock-auth`; `user` is shown (the whole name of
     * the user, in big letters, with the time over it). The window is made
     * and shown, the keyboard is not grabbed yet (that is `lock()`).
     */
    LockScreen(Display& display, std::vector<std::string> command,
               std::string user);
    ~LockScreen();

    LockScreen(const LockScreen&) = delete;
    LockScreen& operator=(const LockScreen&) = delete;

    /**
     * @short Grab the keyboard and the pointer; false if another client holds
     * a grab.
     *
     * It tries for `patience` (a menu that is just closing holds one for a
     * moment). The lock does not exist without the grabs, so the caller ends.
     */
    bool
    lock(std::chrono::milliseconds patience = std::chrono::milliseconds(2000));

    //! Process what has come (events of X, the talk with the program that
    //! checks) and wait at most `timeout_ms` for it (-1 is as long as it takes).
    //! False when the screen is unlocked.
    bool step(int timeout_ms);

    //! `step()` until the screen is unlocked
    void run();

    //! The answer from the entry; what pressing Return does
    void submit();

    //! What the dialog says now
    const std::string& prompt() const { return prompt_text; }
    const std::string& status() const { return status_text; }
    //! The time that is shown over the dialog, "14:05"
    const std::string& clock_text() const { return clock_text_; }

    //! Show the time `now` (the lock does it itself at every minute)
    void update_clock(std::time_t now);
    bool unlocked() const { return done; }
    bool is_asking() const { return asking; }
    Entry& entry();
    ::Window window() const { return cover; }

  private:
    class Dialog;

    void handle_events();
    //! True if there was a result (the try is over)
    bool handle_messages(const std::vector<Message>& messages);
    void handle_result(const Outcome& outcome);
    void start_try();
    void set_status(const std::string& text);
    void set_prompt(const std::string& text, bool hidden);
    void place_dialog();
    void give_focus();
    void unlock();
    void on_visibility(const XEvent& event);

    Display& display;
    AuthSession session;
    std::string user;
    ::Window cover = None;
    Cursor blank_cursor = None;
    std::unique_ptr<Dialog> dialog;
    std::string prompt_text;
    std::string clock_text_;
    Clock::time_point next_clock; //!< when the time is shown again
    std::string status_text;
    bool asking = false; //!< a question waits for the answer
    bool grabbed = false;
    bool focused = false;
    bool done = false;
    //! When the next try starts; the epoch means none is waiting
    Clock::time_point retry_at;
};

} // namespace tiny::app
