#include "runner.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <utility>

#include "spawn.h"

namespace tiny {

namespace {

constexpr std::chrono::milliseconds status_retry{20};

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
    const pid_t pid = spawn_capture(argv, &fd);
    if (pid < 0) {
        return false;
    }
    const unsigned long id = next_id++;
    auto job = std::make_unique<Job>();
    job->pid = pid;
    job->fd = fd;
    job->options = options;
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
    for (;;) {
        const ssize_t n = read(job.fd, buffer, sizeof(buffer));
        if (n > 0) {
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
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
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
    waitpid(job.pid, nullptr, 0); // it was killed, so this does not wait
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
    int status = 0;
    switch (collect(job.pid, &status)) {
        case Ended::No:
            // closed its output but runs: ask again, the timeout ends it
            timers.after(status_retry, [this, id] { finish(id); });
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
        waitpid(job->pid, nullptr, 0);
    }
    jobs.clear();
}

} // namespace tiny
