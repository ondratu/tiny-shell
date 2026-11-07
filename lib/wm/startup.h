#pragma once

#include <X11/Xlib.h>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>

namespace tiny::wm {

//! One application that is starting (freedesktop.org startup notification)
struct StartupSequence {
    using Clock = std::chrono::steady_clock;

    std::string id;      //!< the value of `DESKTOP_STARTUP_ID`
    std::string name;    //!< what the user starts ("Files")
    std::string bin;     //!< the program
    std::string icon;    //!< the icon name
    std::string wmclass; //!< `WM_CLASS` of the window to come, may be empty
    bool silent = false; //!< no feedback to the user
    Clock::time_point deadline;
};

//! A message of the protocol after it is put together
struct StartupMessage {
    std::string command;                       //!< "new", "change" or "remove"
    std::map<std::string, std::string> values; //!< ID, NAME, BIN, ...
};

//! Parse "new: ID=\"a b\" NAME=x"; nothing when it is not a message
std::optional<StartupMessage> parse_startup_message(const std::string& text);

/**
 * @short The applications that are starting.
 *
 * The launcher of an application (the shell, or another program) sends
 * `new:` as client messages to the root window and `remove:` when it gives up;
 * the application ends the sequence by setting `_NET_STARTUP_ID` of its first
 * window, or its window has the `WM_CLASS` that the sequence names. A sequence
 * that nobody ends is dropped after `timeout`, so a program that does not know
 * the protocol does not keep the "busy" cursor for ever. There is no X here,
 * only the decisions, so it can be tested.
 */
class StartupNotification {
  public:
    using Clock = StartupSequence::Clock;

    static constexpr std::chrono::seconds timeout{15};

    //! The most sequences that are kept at the same time
    static constexpr size_t most_sequences = 64;

    /**
     * 20 bytes of a client message of the window `sender`. The first message
     * of a text (`begin`) starts it, the following ones add to it; a zero byte
     * ends it and the message is handled.
     */
    void feed(::Window sender, const char* data, bool begin,
              Clock::time_point now);

    //! Handle a message that is complete
    void handle(const StartupMessage& message, Clock::time_point now);

    //! The shell starts an application itself
    void begin(StartupSequence sequence, Clock::time_point now);

    //! The application has started (or the launch failed); false when there
    //! was no such sequence
    bool finish(const std::string& id);

    //! A window of the class appeared: the oldest sequence that waits for it
    //! ends; false when none does
    bool finish_class(const std::string& instance, const std::string& klass);

    //! Drop the sequences that took too long; true when one was dropped
    bool expire(Clock::time_point now);

    //! Is there a sequence the user should see (a "busy" cursor)
    bool busy() const;

    //! When `expire` has something to do, none without sequences
    std::optional<Clock::time_point> next_deadline() const;

    const std::map<std::string, StartupSequence>& sequences() const
    {
        return running;
    }

    //! The longest text of a message that is put together
    static constexpr size_t longest_message = 4096;

  private:
    std::map<std::string, StartupSequence> running; //!< by id
    //! texts not complete yet, with the number of their beginning
    std::map<::Window, std::pair<unsigned long, std::string>> partial;
    unsigned long begun = 0; //!< counts the beginnings
};

} // namespace tiny::wm
