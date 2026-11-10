#pragma once

#include <string>

#include "app/lock_check.h"

namespace tiny::app {

/**
 * @short The code of `tiny-lock-pwcheck`, the only program of the lock that has
 * privileges.
 *
 * It is set-group-ID `shadow` (Linux) or set-user-ID root (BSD), because only
 * they can read the hash of the password. It checks the password of the user
 * that runs it, never of another one, and it gives up the privileges as soon as
 * it has the hash. It is kept small on purpose; see TODO-LOCK.md.
 */

/**
 * @short Compare the password with the hash, with `crypt()`.
 *
 * `Failed` if the hash is not usable or `crypt()` does not know its format,
 * `Wrong` for an empty password. The comparison takes the same time wherever
 * the two texts differ.
 */
Verdict verify_password(const std::string& hash, const std::string& password);

//! Give up the privileges of the program for good (the group and the user are
//! the real ones). False if it could not be done or if it was undone.
bool drop_privileges();

//! No core dump, no attaching of a debugger by a process of the same user
void protect_memory();

/**
 * @short What `tiny-lock-pwcheck` does, as a function that returns the exit status.
 *
 * `--probe` only says if the account of the user has a password to unlock with.
 * Without it the password comes from the standard input (to the end of the
 * line or of the file). The status is 0 for the right password (or a usable
 * account for `--probe`), 1 for the wrong one, 2 if it can't be checked.
 * `delay_ms` is the wait after a wrong password.
 */
int run_pwcheck(int argc, const char* const argv[], int input,
                unsigned delay_ms);

} // namespace tiny::app
