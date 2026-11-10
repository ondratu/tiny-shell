#include "app/lock_session.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstring>

namespace tiny::app {

AuthSession::AuthSession(std::vector<std::string> command_arg):
    command(std::move(command_arg))
{}

AuthSession::~AuthSession()
{
    stop();
}

bool AuthSession::start()
{
    stop();
    if (command.empty()) {
        problem = "no program to check the password";
        return false;
    }
    int to_child[2];
    int from_child[2];
    if (pipe(to_child) != 0) {
        problem = std::string("pipe: ") + strerror(errno);
        return false;
    }
    if (pipe(from_child) != 0) {
        problem = std::string("pipe: ") + strerror(errno);
        close(to_child[0]);
        close(to_child[1]);
        return false;
    }
    std::vector<char*> arguments;
    arguments.reserve(command.size() + 1);
    for (const std::string& word : command) {
        // execvp() takes char*, but does not change the strings
        arguments.push_back(const_cast<char*>(word.c_str()));
    }
    arguments.push_back(nullptr);

    child = fork();
    if (child < 0) {
        problem = std::string("fork: ") + strerror(errno);
        for (const int fd :
             {to_child[0], to_child[1], from_child[0], from_child[1]}) {
            close(fd);
        }
        return false;
    }
    if (child == 0) {
        dup2(to_child[0], STDIN_FILENO);
        dup2(from_child[1], STDOUT_FILENO);
        for (const int fd :
             {to_child[0], to_child[1], from_child[0], from_child[1]}) {
            close(fd);
        }
        execvp(arguments[0], arguments.data());
        _exit(127);
    }
    close(to_child[0]);
    close(from_child[1]);
    input = to_child[1];
    output = from_child[0];
    // never inherited by the programs that the auth starts
    fcntl(input, F_SETFD, FD_CLOEXEC);
    fcntl(output, F_SETFD, FD_CLOEXEC);
    problem.clear();
    return true;
}

std::vector<Message> AuthSession::read()
{
    std::vector<Message> messages;
    if (output < 0) {
        return messages;
    }
    char bytes[512];
    const ssize_t got = ::read(output, bytes, sizeof bytes);
    if (got < 0 && errno == EINTR) {
        return messages;
    }
    if (got > 0) {
        buffer.feed(bytes, static_cast<size_t>(got));
    }
    while (auto line = buffer.next()) {
        if (auto message = parse_message(*line)) {
            messages.push_back(std::move(*message));
        }
    }
    if (got <= 0 || buffer.overflow()) { // the program has ended
        stop();
    }
    return messages;
}

bool AuthSession::answer(const std::string& text) const
{
    if (input < 0) {
        return false;
    }
    std::string line = format_message(Kind::Answer, text);
    const char* bytes = line.data();
    size_t left = line.size();
    bool done = true;
    // a program that is gone gives EPIPE; the process ignores SIGPIPE
    struct sigaction ignore = {};
    struct sigaction saved = {};
    ignore.sa_handler = SIG_IGN;
    sigemptyset(&ignore.sa_mask);
    sigaction(SIGPIPE, &ignore, &saved);
    while (left > 0) {
        const ssize_t written = write(input, bytes, left);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            done = false;
            break;
        }
        bytes += written;
        left -= static_cast<size_t>(written);
    }
    sigaction(SIGPIPE, &saved, nullptr);
    wipe(line);
    return done;
}

void AuthSession::stop()
{
    for (int* fd : {&input, &output}) {
        if (*fd >= 0) {
            close(*fd);
            *fd = -1;
        }
    }
    buffer.clear();
    if (child > 0) {
        kill(child, SIGKILL);
        while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
        child = -1;
    }
}

int run_to_end(const std::vector<std::string>& command)
{
    if (command.empty()) {
        return -1;
    }
    std::vector<char*> arguments;
    arguments.reserve(command.size() + 1);
    for (const std::string& word : command) {
        arguments.push_back(const_cast<char*>(word.c_str()));
    }
    arguments.push_back(nullptr);
    const pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        const int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            dup2(null,
                 STDOUT_FILENO); // what a probe says is for the exit status
        }
        execvp(arguments[0], arguments.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

} // namespace tiny::app
