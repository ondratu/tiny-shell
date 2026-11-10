// tiny-lock-auth: checks the password for tiny-lock.
//
// It runs one try with the lines of lib/app/lock_protocol.h on its standard
// input and output, tells the result and ends. It has no privileges. It is the
// only program that links libpam (the make variable PAM). With PAM the system
// decides what unlocks (a password, a fingerprint, a token); without it the
// password goes to a program that checks it: `unix_chkpwd` or
// `tiny-lock-pwcheck`.
//
//   tiny-lock-auth [--method auto|pam|checker|pwcheck] [--service name]
//                  [--checker path] [--pwcheck path] [--probe]
//
// --probe checks that the way can be used, prints which it is and ends (0 yes,
// 1 no, the reason is on the standard error). The exit status of a try is 0
// when it says a result, 2 when it could not talk.

#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>

#include "app/lock_auth.h"
#include "app/lock_protocol.h"
#include "spawn.h"

namespace {

constexpr int usage_status = 2;

void usage()
{
    fputs("Usage: tiny-lock-auth [--method auto|pam|checker|pwcheck] "
          "[--service name]\n"
          "                      [--checker path] [--pwcheck path] "
          "[--probe]\n",
          stderr);
}

} // namespace

int main(int argc, char* argv[])
{
    tiny::app::AuthSettings settings;
    bool probe = false;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        const bool has_value = i + 1 < argc;
        if (option == "--probe") {
            probe = true;
        } else if (option == "--method" && has_value) {
            if (!tiny::app::method_from_name(argv[++i], settings.method)) {
                usage();
                return usage_status;
            }
        } else if (option == "--service" && has_value) {
            settings.pam_service = argv[++i];
        } else if (option == "--checker" && has_value) {
            settings.checker = argv[++i];
        } else if (option == "--pwcheck" && has_value) {
            settings.pwcheck = argv[++i];
        } else {
            usage();
            return usage_status;
        }
    }
    if (settings.pwcheck.find('/') == std::string::npos) { // next to us
        settings.pwcheck = tiny::sibling_program(argv[0], settings.pwcheck);
    }

    signal(SIGPIPE, SIG_IGN); // a lock that is gone is an error of write()
    const std::string user = tiny::app::user_name();
    tiny::app::Backend backend;
    std::string why;
    const bool usable =
            !user.empty() &&
            tiny::app::resolve_backend(settings, user, backend, why) &&
            tiny::app::probe_backend(backend, why);
    if (user.empty()) {
        why = "unknown user";
    }
    if (probe) {
        if (!usable) {
            fprintf(stderr, "tiny-lock-auth: %s\n", why.c_str());
            return 1;
        }
        printf("%s\n", backend.description.c_str());
        return 0;
    }

    tiny::app::LineDialog dialog(STDIN_FILENO, STDOUT_FILENO);
    if (!usable) {
        dialog.finish({tiny::app::Verdict::Failed, why});
        return 0;
    }
    dialog.finish(tiny::app::authenticate(backend, user, dialog));
    return 0;
}
