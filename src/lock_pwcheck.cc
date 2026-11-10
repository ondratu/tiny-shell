// tiny-lock-pwcheck: the only program of the screen lock with privileges.
//
// It reads the hash of the password of the user that runs it, gives up the
// privileges and compares the password from its standard input with the hash.
// It says the answer only with the exit status: 0 right, 1 wrong, 2 can't be
// checked. `--probe` tells whether the account has a password to unlock with.
// See TODO-LOCK.md for the rules.
//
// It needs to be set-group-ID shadow (Linux) or set-user-ID root (BSD), which
// `make install` does not do; see doc/install.md.

#include <unistd.h>

#include "app/pwcheck.h"

int main(int argc, char* argv[])
{
    constexpr unsigned delay_ms = 2000; // like pam_unix after a failure
    return tiny::app::run_pwcheck(argc, argv, STDIN_FILENO, delay_ms);
}
