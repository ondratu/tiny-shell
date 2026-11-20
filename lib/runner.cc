#include "runner.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <utility>

#include "spawn.h"

namespace tiny {

namespace {

constexpr std::chrono::milliseconds status_retry{20};
//! How much of the output is read in one call of the loop
constexpr size_t read_budget = size_t{256} * 1024;

enum class Ended { No, Yes, Lost };

//! Whether the child has ended, and how (the handler of SIGCHLD may have
//! collected it already; Lost is a child nobody knows)
Ended collect(pid_t pid, int* status)
{
    const pid_t got = waitpid(pid, status, WNOHANG);
    if (got == pid) {
        return Ended::Yes;
    }
    if (got == 0) {
        return Ended::No;
    }
    return child_ended(pid, status) ? Ended::Yes : Ended::Lost;
}

void kill_session(pid_t pid)
{
    kill(-pid, SIGKILL); // the program is the leader of its own session
    kill(pid, SIGKILL);
}

} // namespace

Runner::Runner(Timers& timers_arg, FdWatch& watches_arg):
    timers(timers_arg), watches(watches_arg)
{}

Runner::~Runner()
{
    cancel_all();
}

bool Runner::run(const std::vector<std::string>& argv, Done done)
{
    return run(argv, std::move(done), Options{});
}

bool Runner::run(const std::vector<std::string>& argv, Done done,
                 const Options& options)
{
    int fd = -1;
    const pid_t pid = spawn_capture(argv, &fd, options.input);
    if (pid < 0) {
        return false;
    }
    const unsigned long id = next_id++;
    auto job = std::make_unique<Job>();
    job->pid = pid;
    job->fd = fd;
    job->options = options;
    job->options.input.clear(); // a password is not kept longer than needed
    job->done = std::move(done);
    job->watch = watches.add(fd, [this, id] { on_readable(id); });
    job->timer = timers.after(options.timeout, [this, id] { on_timeout(id); });
    jobs[id] = std::move(job);
    return true;
}

void Runner::on_readable(unsigned long id)
{
    const auto it = jobs.find(id);
    if (it == jobs.end()) {
        return;
    }
    Job& job = *it->second;
    char buffer[4096];
    // A program that writes without a pause must not keep the loop here: the
    // rest is read at the next round, and the timers (its time limit) run.
    size_t budget = read_budget;
    for (;;) {
        if (budget == 0) {
            return; // still ready, so the loop comes back at once
        }
        const ssize_t n = read(job.fd, buffer, sizeof(buffer));
        if (n > 0) {
            budget -= std::min(budget, static_cast<size_t>(n));
            const size_t room =
                    job.options.max_output > job.result.output.size()
                            ? job.options.max_output - job.result.output.size()
                            : 0;
            const auto got = static_cast<size_t>(n);
            job.result.output.append(buffer, got < room ? got : room);
            job.result.truncated = job.result.truncated || got > room;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        // the same number on Linux, two on some systems
#if EAGAIN != EWOULDBLOCK
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
#else
        if (n < 0 && errno == EAGAIN) {
#endif
            return; // wait for more
        }
        break; // the end of the file, or an error
    }
    job.eof = true;
    watches.remove(job.watch); // it would be ready again and again
    job.watch = 0;
    finish(id);
}

void Runner::on_timeout(unsigned long id)
{
    const auto it = jobs.find(id);
    if (it == jobs.end()) {
        return;
    }
    Job& job = *it->second;
    job.timer = 0;
    job.result.timed_out = true;
    kill_session(job.pid);
    // Not waited for: a program in an uninterruptible sleep (a hung network
    // disk) does not end at once, and the shell must not wait for it. The
    // handler of SIGCHLD collects it when it ends.
    waitpid(job.pid, nullptr, WNOHANG);
    discard(job);
    const Done done = std::move(job.done);
    const RunResult result = std::move(job.result);
    jobs.erase(it);
    if (done) {
        done(result);
    }
}

//! The output has ended: wait for the status of the program
void Runner::finish(unsigned long id)
{
    const auto it = jobs.find(id);
    if (it == jobs.end()) {
        return;
    }
    Job& job = *it->second;
    job.retry = 0; // the timer that called this is gone
    int status = 0;
    switch (collect(job.pid, &status)) {
        case Ended::No:
            // closed its output but runs: ask again, the timeout ends it
            job.retry = timers.after(status_retry, [this, id] { finish(id); });
            return;
        case Ended::Yes:
            if (WIFEXITED(status)) {
                job.result.status = WEXITSTATUS(status);
                job.result.ok = job.result.status == 0;
            }
            break;
        case Ended::Lost:
            break;
    }
    discard(job);
    const Done done = std::move(job.done);
    const RunResult result = std::move(job.result);
    jobs.erase(it);
    if (done) {
        done(result);
    }
}

//! Close what a job holds
void Runner::discard(Job& job)
{
    if (job.watch != 0) {
        watches.remove(job.watch);
        job.watch = 0;
    }
    if (job.timer != 0) {
        timers.cancel(job.timer);
        job.timer = 0;
    }
    if (job.retry != 0) {
        timers.cancel(job.retry);
        job.retry = 0;
    }
    if (job.fd >= 0) {
        close(job.fd);
        job.fd = -1;
    }
}

void Runner::cancel_all()
{
    for (auto& [id, job] : jobs) {
        discard(*job);
        kill_session(job->pid);
        waitpid(job->pid, nullptr, WNOHANG); // see on_timeout()
    }
    jobs.clear();
}

} // namespace tiny
