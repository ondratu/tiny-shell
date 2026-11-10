#pragma once

#include <string>

#include "app/lock_check.h"
#include "app/lock_protocol.h"

namespace tiny::app {

//! How the password is checked
enum class Method {
    Auto,    //!< see `resolve_backend()`
    Pam,     //!< PAM
    Checker, //!< a program like `unix_chkpwd`, set at the build time
    Pwcheck, //!< `tiny-lock-pwcheck`
};

//! The name of a method in the configuration: auto, pam, checker, pwcheck
bool method_from_name(const std::string& name, Method& method);
const char* method_name(Method method);

//! What `[lock]` of the configuration says about the check
struct AuthSettings {
    Method method = Method::Auto;
    std::string pam_service = "tiny-lock";
    std::string checker; //!< "" is `unix_chkpwd`, searched
    std::string pwcheck = "tiny-lock-pwcheck"; //!< a name or a path
};

//! A way of checking that is chosen
struct Backend {
    Method method = Method::Auto; //!< never Auto
    std::string pam_service;
    Checker checker;
    std::string description; //!< for a message: "PAM, service tiny-lock"
    //! The wait after a wrong password, for a checker that has none of its own
    //! (`unix_chkpwd`; PAM and `tiny-lock-pwcheck` wait themselves)
    unsigned wrong_delay_ms = 0;
};

/**
 * @short Choose the way to check the password.
 *
 * `Auto` takes, in this order:
 *
 *  1. PAM, if it is built in and it can work without the right to read the
 *     hash: Linux-PAM has `unix_chkpwd` for that, and a process that can read
 *     the hash needs nothing. (OpenPAM of the BSD systems has no helper: PAM
 *     would say "wrong" to the right password there.)
 *  2. the checker (`unix_chkpwd`), when there is no PAM in this build.
 *  3. `tiny-lock-pwcheck`, if it has the set-user-ID or set-group-ID bit.
 *
 * It fails (false, with the reason) when there is nothing, because a lock that
 * can't be unlocked is worse than no lock. A method that is named is taken as
 * it is, as long as it can be used at all.
 */
bool resolve_backend(const AuthSettings& settings, const std::string& user,
                     Backend& backend, std::string& why);

//! Whether the way can be used now: the program is there, the account has a
//! password to unlock with (`tiny-lock-pwcheck --probe`)
bool probe_backend(const Backend& backend, std::string& why);

/**
 * @short Check one try: ask for the password (or what PAM asks) and tell.
 *
 * `Failed` with the text when the user cancelled or nothing could be checked.
 */
Outcome authenticate(const Backend& backend, const std::string& user,
                     Dialog& dialog);

} // namespace tiny::app
