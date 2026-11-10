#pragma once

#include <X11/Xlib.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "app/lock_session.h"
#include "container.h"
#include "entry.h"
#include "label.h"

namespace tiny::app {

//! What the lock shows and when it goes dark
struct LockOptions {
    //! `strftime()` format of the date under the time, "" is no date
    std::string date_format = "%a %e %B";
    //! After this time without input the screen is black; zero is never
    std::chrono::milliseconds blank_after = std::chrono::minutes(1);
};

/**
 * @short The date of the format of the clock of the panel: the format without
 * the time of the day.
 *
 * The directives of the time (`%H %I %k %l %M %S %p %P %R %T %r %X %Z %z %s`)
 * and the blanks and separators that were around them are left out; when
 * nothing is left, the format is the default one of `LockOptions`.
 */
std::string date_format_of(const std::string& clock_format);

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
     * What the screen shows. At the start it is `Idle`: the time, the date and
     * a text that says what to do. A click or a key opens the `Dialog` (and
     * only then the check of the password starts, so a fingerprint or a face
     * is looked for when somebody is there); Escape goes back to `Idle`. After
     * `LockOptions::blank_after` without input in `Idle` the screen is `Black`,
     * and the next input wakes it to `Idle`.
     */
    enum class State { Idle, Dialog, Black };

    /**
     * `command` starts `tiny-lock-auth`; `user` is shown (the whole name of
     * the user, in big letters, with the time over it). The window is made
     * and shown, the keyboard is not grabbed yet (that is `lock()`).
     */
    LockScreen(Display& display, std::vector<std::string> command,
               std::string user, LockOptions options = LockOptions());
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

    State state() const { return state_; }

    //! Open the dialog and start the check (what a key or a click does in
    //! `Idle`); Escape does `hide_dialog()`
    void show_dialog();

    //! Close the dialog, stop the check and forget what was typed
    void hide_dialog();

    /**
     * @short Fit the lock to the screen as it is now: the window covers all of
     * it and the dialog is in the middle of the primary monitor.
     *
     * Called when RandR says the monitors changed (one was connected, or the
     * layout was changed), so no part of the screen is left open.
     */
    void screen_changed();

    //! Make the screen black, and `wake()` shows the time again
    void blank();
    void wake();

    /**
     * @short What `step()` does with an event of the keyboard or the pointer.
     *
     * Returns true when the event is used up. In `Black` any input wakes the
     * screen, in `Idle` a key or a click opens the dialog and a move of the
     * pointer only restarts the time to the black screen; none of them is
     * typed into the entry. In the `Dialog` a key is for the entry (also one
     * that came to the window of the lock instead of the entry).
     */
    bool handle_input(XEvent& event);

    //! What the dialog says now
    const std::string& prompt() const { return prompt_text; }
    const std::string& status() const { return status_text; }
    //! The time that is shown over the dialog, "14:05"
    const std::string& clock_text() const { return clock_text_; }

    //! The date under the time, "Tue 10 November"; empty without a date
    const std::string& date_text() const { return date_text_; }

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
    void set_state(State state);
    void touch(); //!< input: the time to the black screen starts again

    Display& display;
    AuthSession session;
    std::string user;
    ::Window cover = None;
    Cursor blank_cursor = None;
    std::unique_ptr<Dialog> dialog;
    std::string prompt_text;
    std::string clock_text_;
    std::string date_text_;
    LockOptions options;
    State state_ = State::Idle;
    //! When the screen goes black; the epoch means it does not
    Clock::time_point blank_at;
    Clock::time_point next_clock; //!< when the time is shown again
    std::string status_text;
    bool asking = false; //!< a question waits for the answer
    bool grabbed = false;
    std::optional<int> randr_base; //!< the first event of RandR, if it has any
    bool focused = false;
    bool done = false;
    //! When the next try starts; the epoch means none is waiting
    Clock::time_point retry_at;
};

} // namespace tiny::app
