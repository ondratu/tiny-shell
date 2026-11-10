#pragma once

#include <sys/types.h>

#include <string>
#include <vector>

#include "app/lock_protocol.h"

namespace tiny::app {

/**
 * @short A running `tiny-lock-auth` that the lock talks to.
 *
 * It starts the program with a pipe to its standard input and from its
 * standard output. `read()` is for when `descriptor()` can be read (`poll()`);
 * it never waits for a whole line.
 */
class AuthSession {
  public:
    //! The program and its arguments, as `execvp()` takes them
    explicit AuthSession(std::vector<std::string> command);
    AuthSession(const AuthSession&) = delete;
    AuthSession& operator=(const AuthSession&) = delete;

    //! Ends the program (it can't be left with the screen locked)
    ~AuthSession();

    //! Start the program; false (with `error()`) if it could not be started
    bool start();

    bool running() const { return output >= 0; }

    //! The descriptor to wait on, -1 when nothing runs
    int descriptor() const { return output; }

    /**
     * @short Take what the program has said.
     *
     * The whole lines of one read. When the program has ended (or wrote more
     * than a line may have) `running()` is false after it, and the program is
     * gone.
     */
    std::vector<Message> read();

    //! Send the answer to a question; false if the program is gone
    bool answer(const std::string& text) const;

    //! End the program now
    void stop();

    const std::string& error() const { return problem; }

  private:
    std::vector<std::string> command;
    pid_t child = -1;
    int input = -1;  //!< what we write
    int output = -1; //!< what we read
    LineBuffer buffer;
    std::string problem;
};

/**
 * @short Run the program to its end and give its exit status.
 *
 * -1 when it can't be started or ends in another way than by exit. Its
 * standard input is empty and its standard output is thrown away; the error
 * output stays where it is.
 */
int run_to_end(const std::vector<std::string>& command);

} // namespace tiny::app
