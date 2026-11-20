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

//! The longest text for the standard input: what a pipe takes at once
constexpr size_t max_input = 4096;

// The statuses of the children that ended, a ring, written by the handler of
// SIGCHLD
volatile sig_atomic_t kept_pid[kept_statuses] = {};
volatile sig_atomic_t kept_status[kept_statuses] = {};
volatile sig_atomic_t kept_next = 0;

void on_sigchld(int)
{
    const int saved_errno = errno;
    int status = 0;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        const sig_atomic_t slot = kept_next;
        kept_status[slot] = status;
        kept_pid[slot] = pid;
        kept_next = (slot + 1) % kept_statuses;
    }
    errno = saved_errno;
}

// The child reports a failed chdir or exec through the pipe (closed by a
// good exec).
[[noreturn]] void exec_child(const std::vector<std::string>& argv,
                             const std::string& directory,
                             const std::vector<std::string>& environment,
                             int report, int output, int input)
{
    setsid();

    if (output >= 0) { // the standard output goes to the pipe, nothing else
        const int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            dup2(null, STDERR_FILENO);
            if (null > STDERR_FILENO) {
                close(null); // the program does not get one more descriptor
            }
        }
        dup2(output, STDOUT_FILENO);
        if (input >= 0) { // the text that was given to the program
            dup2(input, STDIN_FILENO);
        }
    }

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

namespace {

//! The read end of a pipe that has `text` in it and nothing more coming: the
//! standard input of a program. The text fits in the pipe, so nothing waits.
int input_pipe(const std::string& text)
{
    int fds[2];
    if (text.size() > max_input || pipe2(fds, O_CLOEXEC) < 0) {
        if (text.size() > max_input) {
            errno = EMSGSIZE;
        }
        return -1;
    }
    const ssize_t written = write(fds[1], text.data(), text.size());
    const int saved = errno;
    close(fds[1]);
    if (written != static_cast<ssize_t>(text.size())) {
        close(fds[0]);
        errno = written < 0 ? saved : EIO;
        return -1;
    }
    return fds[0];
}

//! `spawn()`; with `output` not null the standard output of the program is a
//! pipe and `*output` is the end that reads it; `input` is what the program
//! reads from its standard input (null: nothing)
pid_t spawn_with_output(const std::vector<std::string>& argv,
                        const std::string& directory,
                        const std::vector<std::string>& environment,
                        int* output, const std::string* input = nullptr)
{
    if (argv.empty() || argv[0].empty()) {
        errno = EINVAL;
        return -1;
    }
    int in = -1;
    if (input != nullptr && !input->empty()) {
        in = input_pipe(*input);
        if (in < 0) {
            return -1;
        }
    }

    int report[2];
    if (pipe2(report, O_CLOEXEC) < 0) {
        const int saved = errno;
        if (in >= 0) {
            close(in);
        }
        errno = saved;
        return -1;
    }
    int data[2] = {-1, -1};
    if (output != nullptr && pipe2(data, O_CLOEXEC) < 0) {
        const int saved = errno;
        close(report[0]);
        close(report[1]);
        if (in >= 0) {
            close(in);
        }
        errno = saved;
        return -1;
    }

    const pid_t pid = fork();
    if (pid == 0) {
        close(report[0]);
        if (data[0] >= 0) {
            close(data[0]);
        }
        exec_child(argv, directory, environment, report[1], data[1], in);
    }

    const int fork_errno = errno;
    close(report[1]);
    if (data[1] >= 0) {
        close(data[1]);
    }
    if (in >= 0) {
        close(in);
    }

    if (pid < 0) {
        close(report[0]);
        if (data[0] >= 0) {
            close(data[0]);
        }
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
        if (data[0] >= 0) {
            close(data[0]);
        }
        errno = error;
        return -1;
    }
    if (output != nullptr) {
        fcntl(data[0], F_SETFL, fcntl(data[0], F_GETFL) | O_NONBLOCK);
        *output = data[0];
    }
    return pid;
}

} // namespace

pid_t spawn(const std::vector<std::string>& argv, const std::string& directory,
            const std::vector<std::string>& environment)
{
    return spawn_with_output(argv, directory, environment, nullptr);
}

pid_t spawn_capture(const std::vector<std::string>& argv, int* output,
                    const std::string& input)
{
    if (output == nullptr) {
        errno = EINVAL;
        return -1;
    }
    return spawn_with_output(argv, "", {}, output, &input);
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

namespace {

//! The program called `name` in the directories of PATH, or `name` itself
std::string find_in_path(const std::string& name)
{
    const char* path = getenv("PATH");
    if (path == nullptr || name.empty()) {
        return name;
    }
    const std::string dirs = path;
    size_t start = 0;
    while (start <= dirs.size()) {
        size_t end = dirs.find(':', start);
        if (end == std::string::npos) {
            end = dirs.size();
        }
        if (end > start) {
            const std::string candidate =
                    dirs.substr(start, end - start) + "/" + name;
            if (access(candidate.c_str(), X_OK) == 0) {
                return candidate;
            }
        }
        start = end + 1;
    }
    return name;
}

} // namespace

std::string sibling_program(const char* argv0, const std::string& name)
{
    char resolved[PATH_MAX];
    std::string found = argv0;
    if (found.find('/') == std::string::npos) { // started from PATH
        found = find_in_path(argv0);
    }
    const std::string program =
            realpath(found.c_str(), resolved) != nullptr ? resolved : found;
    const size_t slash = program.rfind('/');
    if (slash == std::string::npos) {
        return name;
    }
    const std::string sibling = program.substr(0, slash + 1) + name;
    return access(sibling.c_str(), X_OK) == 0 ? sibling : name;
}

bool child_ended(pid_t pid, int* status)
{
    if (pid <= 0) {
        return false;
    }
    for (int i = 0; i < kept_statuses; ++i) {
        if (kept_pid[i] == pid) {
            if (status != nullptr) {
                *status = static_cast<int>(kept_status[i]);
            }
            return true;
        }
    }
    return false;
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
