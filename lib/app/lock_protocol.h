#pragma once

#include <optional>
#include <string>

#include "app/lock_check.h"

namespace tiny::app {

/**
 * @short The talk between `tiny-lock` and `tiny-lock-auth`: one line each.
 *
 * `tiny-lock-auth` asks and tells, `tiny-lock` answers:
 *
 * ```
 * auth -> lock   PROMPT_HIDDEN Password:     ask, the answer is not shown
 *                PROMPT_VISIBLE Code:        ask, the answer is shown
 *                INFO Place your finger      tell
 *                ERROR Too many tries        tell
 *                RESULT OK                   the end: unlock
 *                RESULT WRONG                the end: the password is wrong
 *                RESULT FAILED text          the end: it could not be checked
 * lock -> auth   ANSWER text                 the answer to a prompt
 *                CANCEL                      no answer (the program ends)
 * ```
 *
 * One program runs one try; after a result it ends and `tiny-lock` starts it
 * again. A line has no newline in the text (it is a space there).
 */
enum class Kind {
    PromptHidden,
    PromptVisible,
    Info,
    Error,
    Result,
    Answer,
    Cancel,
};

struct Message {
    Kind kind = Kind::Info;
    std::string text;
};

//! The line of the message with the newline at its end
std::string format_message(Kind kind, const std::string& text = "");

//! The message of the line (with or without the newline); nothing if the line
//! is not one
std::optional<Message> parse_message(const std::string& line);

//! `RESULT ...` of the outcome and the outcome of the text of `RESULT`
Message result_message(const Outcome& outcome);
Outcome outcome_of(const std::string& result_text);

/**
 * @short Collects bytes and gives the lines, for a reader that does not block.
 *
 * A line longer than `max_line` is dropped (`overflow()` says so), so that a
 * program that does not stop writing can't use all the memory.
 */
class LineBuffer {
  public:
    static constexpr size_t max_line = 4096;

    void feed(const char* bytes, size_t size);

    //! The next full line without its newline
    std::optional<std::string> next();

    //! Whether a line was too long
    bool overflow() const { return too_long; }

    //! Overwrite the rest (the text of an answer is a password)
    void clear();

  private:
    std::string pending;
    bool too_long = false;
};

/**
 * @short What the authentication asks and tells the user.
 *
 * `tiny-lock-auth` implements it with the lines above (`LineDialog`), the
 * tests with a script of answers.
 */
class Dialog {
  public:
    Dialog() = default;
    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;
    virtual ~Dialog() = default;

    //! Ask; false if there is no answer (the user's side is gone or cancelled)
    virtual bool ask(bool hidden, const std::string& prompt,
                     std::string& answer) = 0;

    //! Tell the user something (`error` for a message that is an error)
    virtual void say(bool error, const std::string& text) = 0;
};

//! The `Dialog` over two file descriptors (`input` for lines of `tiny-lock`,
//! `output` for ours)
class LineDialog : public Dialog {
  public:
    LineDialog(int input_fd, int output_fd);
    ~LineDialog() override;

    bool ask(bool hidden, const std::string& prompt,
             std::string& answer) override;
    void say(bool error, const std::string& text) override;

    //! Tell the end of the try
    void finish(const Outcome& outcome);

  private:
    bool write_line(const std::string& line) const;

    int input;
    int output;
    LineBuffer buffer;
};

} // namespace tiny::app
