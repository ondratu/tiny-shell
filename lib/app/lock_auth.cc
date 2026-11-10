#include "app/lock_auth.h"

#include <unistd.h>

#include "app/lock_pam.h"

namespace tiny::app {

bool method_from_name(const std::string& name, Method& method)
{
    for (const Method candidate :
         {Method::Auto, Method::Pam, Method::Checker, Method::Pwcheck}) {
        if (name == method_name(candidate)) {
            method = candidate;
            return true;
        }
    }
    return false;
}

const char* method_name(Method method)
{
    switch (method) {
        case Method::Pam:
            return "pam";
        case Method::Checker:
            return "checker";
        case Method::Pwcheck:
            return "pwcheck";
        default:
            return "auto";
    }
}

namespace {

Backend pam_backend(const AuthSettings& settings)
{
    Backend backend;
    backend.method = Method::Pam;
    backend.pam_service = settings.pam_service;
    backend.description = "PAM, service " + settings.pam_service;
    return backend;
}

Backend checker_backend(const AuthSettings& settings, const std::string& user)
{
    Backend backend;
    backend.method = Method::Checker;
    backend.checker = unix_chkpwd(settings.checker, user);
    backend.description = backend.checker.path;
    backend.wrong_delay_ms =
            2000; // like pam_unix, which waits in the PAM layer
    return backend;
}

Backend pwcheck_backend(const AuthSettings& settings)
{
    Backend backend;
    backend.method = Method::Pwcheck;
    backend.checker = tiny_pwcheck(settings.pwcheck);
    backend.description = backend.checker.path;
    return backend;
}

} // namespace

bool resolve_backend(const AuthSettings& settings, const std::string& user,
                     Backend& backend, std::string& why)
{
    switch (settings.method) {
        case Method::Pam:
            if (!pam_built()) {
                why = "[lock] auth = pam, but this build has no PAM (make "
                      "PAM=yes)";
                return false;
            }
            backend = pam_backend(settings);
            return true;
        case Method::Checker:
            backend = checker_backend(settings, user);
            if (!can_run(backend.checker.path)) {
                why = "[lock] auth = checker, but there is no unix_chkpwd "
                      "(set [lock] checker)";
                return false;
            }
            return true;
        case Method::Pwcheck:
            backend = pwcheck_backend(settings);
            if (!can_run(backend.checker.path)) {
                why = "[lock] auth = pwcheck, but " + settings.pwcheck +
                      " can't be run";
                return false;
            }
            return true;
        default:
            break;
    }

    const Checker chkpwd = unix_chkpwd(settings.checker, user);
    const bool helper = can_run(chkpwd.path);
    // a process that reads the hash itself (root, a system without shadow)
    const bool reads_hash = hash_usable(password_hash(user));
    if (pam_built() && (helper || reads_hash)) {
        backend = pam_backend(settings);
        return true;
    }
    if (helper && !pam_built()) {
        backend = checker_backend(settings, user);
        return true;
    }
    const Backend pwcheck = pwcheck_backend(settings);
    if (has_privileges(pwcheck.checker.path)) {
        backend = pwcheck;
        return true;
    }
    why = "there is no way to check the password: install PAM and "
          "unix_chkpwd, or give tiny-lock-pwcheck the set-user-ID or "
          "set-group-ID bit (see doc/install.md); [lock] auth = pam takes PAM "
          "as it is";
    if (pam_built()) {
        why += ". PAM is built in, but it is not used: this user can't read "
               "the hash of the password (and there is no unix_chkpwd), so "
               "PAM would refuse the right password; run `make "
               "install-setuid`";
    }
    return false;
}

bool probe_backend(const Backend& backend, std::string& why)
{
    switch (backend.method) {
        case Method::Pam:
            // what PAM asks is known only when it runs; a configuration that
            // denies everybody can be seen
            if (pam_service_denies(backend.pam_service, pam_directories())) {
                why = "PAM would refuse every password: the service " +
                      backend.pam_service +
                      " has no file and \"other\" denies everybody; add "
                      "/etc/pam.d/" +
                      backend.pam_service + " or set [lock] pam_service";
                return false;
            }
            return true;
        case Method::Checker:
            if (!can_run(backend.checker.path)) {
                why = "can't run " + backend.checker.path;
                return false;
            }
            return true;
        default: {
            Checker probe = backend.checker;
            probe.arguments = {"--probe"};
            if (run_checker(probe, "").verdict != Verdict::Right) {
                why = "the account has no password to unlock the screen with, "
                      "or " +
                      backend.checker.path + " has no right to read it";
                return false;
            }
            return true;
        }
    }
}

Outcome authenticate(const Backend& backend, const std::string& user,
                     Dialog& dialog)
{
    if (backend.method == Method::Pam) {
        return check_pam(user, backend.pam_service, dialog);
    }
    std::string password;
    if (!dialog.ask(true, "Password:", password)) {
        wipe(password);
        return {Verdict::Failed, "cancelled"};
    }
    if (password.empty()) { // an empty password is never right
        return {Verdict::Wrong, ""};
    }
    const Outcome outcome = run_checker(backend.checker, password);
    wipe(password);
    if (outcome.verdict == Verdict::Wrong && backend.wrong_delay_ms > 0) {
        // a try that is wrong costs time, or a keyboard that types for itself
        // could guess as fast as the hash is checked
        usleep(backend.wrong_delay_ms * 1000);
    }
    return outcome;
}

} // namespace tiny::app
