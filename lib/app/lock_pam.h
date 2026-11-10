#pragma once

#include <string>

#include "app/lock_check.h"
#include "app/lock_protocol.h"

namespace tiny::app {

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
