#include "spawn.h"

#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <cctype>
#include <climits>
#include <cerrno>
#include <cstdlib>
#include <csignal>

namespace tiny {

namespace {

void on_sigchld(int)
{
    const int saved_errno = errno;
    while (waitpid(-1, nullptr, WNOHANG) > 0) {}
    errno = saved_errno;
}

// The child reports a failed chdir or exec through the pipe (closed by a
// good exec).
[[noreturn]] void exec_child(const std::vector<std::string>& argv,
                             const std::string& directory,
                             const std::vector<std::string>& environment,
                             int report)
{
    setsid();

    // exec keeps the mask of blocked signals
    sigset_t none;
    sigemptyset(&none);
    sigprocmask(SIG_SETMASK, &none, nullptr);

    std::vector<char*> args;
    args.reserve(argv.size() + 1);
    for (const auto& arg : argv) {
        args.push_back(const_cast<char*>(arg.c_str()));
    }
    args.push_back(nullptr);

    for (const std::string& change : environment) {
        const size_t equal = change.find('=');
        if (equal == std::string::npos) {
            unsetenv(change.c_str());
        } else {
            setenv(change.substr(0, equal).c_str(), change.c_str() + equal + 1,
                   1);
        }
    }

    if (directory.empty() || chdir(directory.c_str()) == 0) {
        execvp(args[0], args.data());
    }

    // nothing more can be done in the child if this write fails
    const ssize_t written = write(report, &errno, sizeof(errno));
    static_cast<void>(written);
    _exit(127);
}

} // namespace

pid_t spawn(const std::vector<std::string>& argv, const std::string& directory,
            const std::vector<std::string>& environment)
{
    if (argv.empty() || argv[0].empty()) {
        errno = EINVAL;
        return -1;
    }

    int report[2];
    if (pipe2(report, O_CLOEXEC) < 0) {
        return -1;
    }

    const pid_t pid = fork();
    if (pid == 0) {
        close(report[0]);
        exec_child(argv, directory, environment, report[1]);
    }

    const int fork_errno = errno;
    close(report[1]);

    if (pid < 0) {
        close(report[0]);
        errno = fork_errno;
        return -1;
    }

    int error = 0;
    ssize_t got = 0;
    do {
        got = read(report[0], &error, sizeof(error));
    } while (got < 0 && errno == EINTR);
    close(report[0]);

    if (got == static_cast<ssize_t>(sizeof(error))) {
        // SIGCHLD handler may have collected it already
        waitpid(pid, nullptr, 0);
        errno = error;
        return -1;
    }
    return pid;
}

pid_t spawn_shell(const std::string& command)
{
    return spawn({"/bin/sh", "-c", command});
}

pid_t spawn_command(const std::string& text)
{
    // everything with a meaning for the shell, besides the blanks
    const bool plain = text.find_first_of("\"'\\`$&|;<>()[]{}*?!#~=%\n") ==
                       std::string::npos;
    if (!plain) {
        return spawn_shell(text);
    }

    std::vector<std::string> argv;
    std::string word;
    for (const char c : text + ' ') {
        if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            if (!word.empty()) {
                argv.push_back(std::move(word));
                word.clear();
            }
        } else {
            word += c;
        }
    }
    return spawn(argv);
}

std::string sibling_program(const char* argv0, const std::string& name)
{
    char resolved[PATH_MAX];
    const std::string program =
            realpath(argv0, resolved) != nullptr ? resolved : argv0;
    const size_t slash = program.rfind('/');
    if (slash == std::string::npos) {
        return name;
    }
    const std::string sibling = program.substr(0, slash + 1) + name;
    return access(sibling.c_str(), X_OK) == 0 ? sibling : name;
}

void reap_children()
{
    struct sigaction action = {};
    action.sa_handler = on_sigchld;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &action, nullptr);
}

} // namespace tiny
