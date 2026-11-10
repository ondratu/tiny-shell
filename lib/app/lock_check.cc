#include "app/lock_check.h"

#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cerrno>
#include <cstring>

#ifdef __has_include
#if __has_include(<shadow.h>)
#include <shadow.h>
#define TINY_HAVE_SHADOW 1
#endif
#endif

#ifndef TINY_UNIX_CHKPWD
#define TINY_UNIX_CHKPWD ""
#endif

namespace tiny::app {

namespace {

//! What `unix_chkpwd` returns for a wrong password: PAM_AUTH_ERR of Linux-PAM
constexpr int unix_chkpwd_wrong = 7;

//! The status of the child when `exec` fails
constexpr int exec_failed = 127;

bool write_all(int fd, const char* bytes, size_t left)
{
    while (left > 0) {
        const ssize_t written = write(fd, bytes, left);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        bytes += written;
        left -= static_cast<size_t>(written);
    }
    return true;
}

} // namespace

void wipe(char* bytes, size_t size)
{
    volatile char* target = bytes;
    for (size_t i = 0; i < size; ++i) {
        target[i] = 0;
    }
}

void wipe(std::string& text)
{
    wipe(text.data(), text.size());
    text.clear();
}

std::string user_name()
{
    const passwd* entry = getpwuid(getuid());
    return entry != nullptr ? entry->pw_name : "";
}

std::string full_name_from(const std::string& gecos, const std::string& login)
{
    std::string name = gecos.substr(0, gecos.find(','));
    const size_t first = name.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return login;
    }
    name.erase(0, first);
    name.erase(name.find_last_not_of(" \t") + 1);
    return name;
}

std::string user_full_name()
{
    const passwd* entry = getpwuid(getuid());
    if (entry == nullptr) {
        return "";
    }
    return full_name_from(entry->pw_gecos != nullptr ? entry->pw_gecos : "",
                          entry->pw_name);
}

std::string password_hash(const std::string& user)
{
#ifdef TINY_HAVE_SHADOW
    if (const spwd* shadow = getspnam(user.c_str());
        shadow != nullptr && shadow->sp_pwdp != nullptr) {
        return shadow->sp_pwdp;
    }
#endif
    // BSD: the hash is in the passwd entry, and only the user 0 sees it
    if (const passwd* entry = getpwnam(user.c_str());
        entry != nullptr && entry->pw_passwd != nullptr) {
        return entry->pw_passwd;
    }
    return "";
}

bool hash_usable(const std::string& hash)
{
    return !hash.empty() && hash[0] != '!' && hash[0] != '*' && hash != "x";
}

Outcome run_checker(const Checker& checker, const std::string& password)
{
    if (checker.path.empty()) {
        return {Verdict::Failed, "no program to check the password"};
    }
    int input[2];
    if (pipe(input) != 0) {
        return {Verdict::Failed, std::string("pipe: ") + strerror(errno)};
    }
    // before fork: the child may only call what is safe after it
    std::vector<char*> arguments;
    arguments.push_back(const_cast<char*>(checker.path.c_str()));
    for (const std::string& argument : checker.arguments) {
        arguments.push_back(const_cast<char*>(argument.c_str()));
    }
    arguments.push_back(nullptr);
    char* const no_environment[] = {nullptr};

    const pid_t pid = fork();
    if (pid < 0) {
        close(input[0]);
        close(input[1]);
        return {Verdict::Failed, std::string("fork: ") + strerror(errno)};
    }
    if (pid == 0) {
        dup2(input[0], STDIN_FILENO);
        close(input[0]);
        close(input[1]);
        const int null = open("/dev/null", O_WRONLY);
        if (null >= 0) {
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
        }
        execve(checker.path.c_str(), arguments.data(), no_environment);
        _exit(exec_failed);
    }
    close(input[0]);
    // A checker that ends before it reads everything is not an error of ours:
    // the write fails with EPIPE instead of killing us with SIGPIPE.
    struct sigaction ignore = {};
    struct sigaction saved = {};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGPIPE, &ignore, &saved);
    write_all(input[1], password.c_str(),
              password.size() + (checker.terminating_zero ? 1 : 0));
    close(input[1]);
    sigaction(SIGPIPE, &saved, nullptr);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return {Verdict::Failed,
                    std::string("waitpid: ") + strerror(errno)};
        }
    }
    if (!WIFEXITED(status)) {
        return {Verdict::Failed, "the program that checks was killed"};
    }
    const int code = WEXITSTATUS(status);
    if (code == 0) {
        return {Verdict::Right, ""};
    }
    if (code == checker.wrong_status) {
        return {Verdict::Wrong, ""};
    }
    if (code == exec_failed) {
        return {Verdict::Failed, "can't run " + checker.path};
    }
    return {Verdict::Failed,
            checker.path + " returned " + std::to_string(code)};
}

bool can_run(const std::string& path)
{
    struct stat info;
    return !path.empty() && stat(path.c_str(), &info) == 0 &&
           S_ISREG(info.st_mode) && access(path.c_str(), X_OK) == 0;
}

bool has_privileges(const std::string& path)
{
    struct stat info;
    return can_run(path) && stat(path.c_str(), &info) == 0 &&
           (info.st_mode & (S_ISUID | S_ISGID)) != 0;
}

Checker unix_chkpwd(const std::string& path, const std::string& user)
{
    Checker checker;
    checker.arguments = {user, "nonull"};
    checker.wrong_status = unix_chkpwd_wrong;
    checker.terminating_zero = true;
    if (!path.empty()) {
        checker.path = path;
        return checker;
    }
    for (const char* candidate :
         {TINY_UNIX_CHKPWD, "/usr/sbin/unix_chkpwd", "/sbin/unix_chkpwd",
          "/usr/bin/unix_chkpwd", "/usr/lib/unix_chkpwd"}) {
        if (can_run(candidate)) {
            checker.path = candidate;
            break;
        }
    }
    return checker;
}

Checker tiny_pwcheck(const std::string& path)
{
    Checker checker;
    checker.path = path;
    checker.wrong_status = 1;
    return checker;
}

} // namespace tiny::app
