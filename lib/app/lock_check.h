#pragma once

#include <string>
#include <vector>

namespace tiny::app {

//! The answer of a check of a password
enum class Verdict : char {
    Right = 'R',  //!< the password is the right one
    Wrong = 'W',  //!< the password is not the right one
    Failed = 'F', //!< it could not be checked (nothing to ask, no program...)
};

struct Outcome {
    Verdict verdict = Verdict::Failed;
    std::string text; //!< what to tell the user, or what went wrong
};

//! Overwrite the text with zeros (the compiler can't remove it) and empty it
void wipe(std::string& text);

//! The name of the user that runs the program. It comes from the user id, not
//! from `$USER`: the environment can say anything.
std::string user_name();

/**
 * @short A program that checks a password it reads from its standard input.
 *
 * It gets no environment and its output is thrown away; the answer is the
 * exit status: 0 is the right password, `wrong_status` the wrong one, anything
 * else is a failure.
 */
struct Checker {
    std::string path;
    std::vector<std::string> arguments; //!< without the name of the program
    int wrong_status = 1;
    //! What stdin gets after the password: Linux-PAM's `unix_chkpwd` wants a
    //! terminating zero, `tiny-lock-pwcheck` reads to the end
    bool terminating_zero = false;
};

/**
 * @short Run the checker with the password.
 *
 * `Failed` when it can't be started or ends in another way than by exit.
 */
Outcome run_checker(const Checker& checker, const std::string& password);

//! `unix_chkpwd` of Linux-PAM for the user. `path` is searched when empty:
//! the one set at the build (`TINY_UNIX_CHKPWD`), then the usual directories.
//! The path is empty if there is no such program.
Checker unix_chkpwd(const std::string& path, const std::string& user);

//! `tiny-lock-pwcheck`, searched for by `path` (or next to the running program
//! when `path` is a name only)
Checker tiny_pwcheck(const std::string& path);

//! Whether the file can be run and has the set-user-ID or set-group-ID bit
bool has_privileges(const std::string& path);

//! Whether the file can be run
bool can_run(const std::string& path);

} // namespace tiny::app
