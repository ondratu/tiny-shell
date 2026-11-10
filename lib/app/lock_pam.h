#pragma once

#include <string>
#include <vector>

#include "app/lock_check.h"
#include "app/lock_protocol.h"

namespace tiny::app {

/**
 * @short Whether the PAM configuration of the service denies everybody.
 *
 * A service with no file in the directories uses `other`. On Fedora and Arch
 * that is `pam_deny`, so the right password would be refused and a lock that
 * can't be unlocked is worse than none. It is told only when the file of the
 * service (or `other`) has `auth` lines and every one of them is `pam_deny`
 * (an `include`, `substack` or `@include` leads to the real configuration).
 * `directories` are the places of the files (`/etc/pam.d`, `/usr/lib/pam.d`).
 */
bool pam_service_denies(const std::string& service,
                        const std::vector<std::string>& directories);

//! The directories where the system keeps the files of the PAM services
std::vector<std::string> pam_directories();

//! Whether this build has PAM (the make variable `PAM`, see Makefile)
bool pam_built();

/**
 * @short Check the user with PAM (`pam_authenticate`).
 *
 * Everything the system has set up for the service works: a fingerprint,
 * Howdy, a token. The questions of the modules go to the user through the
 * dialog, so there can be more than one. The service has no file of its own
 * needed: PAM uses its "other" for a service that has none.
 *
 * `Wrong` for a failure of the authentication (a wrong password, too many
 * tries), `Failed` when PAM can't be used at all or the user cancelled.
 */
Outcome check_pam(const std::string& user, const std::string& service,
                  Dialog& dialog);

} // namespace tiny::app
