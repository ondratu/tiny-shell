#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <sys/types.h>
#include <vector>

#include "fd_watch.h"
#include "timers.h"

namespace tiny {

//! What a program that `Runner` ran wrote and how it ended
struct RunResult {
    bool ok = false;        //!< it ended with the status 0
    int status = -1;        //!< the exit status, -1 when a signal ended it
    bool timed_out = false; //!< it did not end in time and was killed
    bool truncated = false; //!< it wrote more than the limit, the rest is lost
    std::string output;     //!< its standard output
};

/**
 * @short Run programs in the event loop and collect their output.
 *
 * `run()` returns at once; the program is read by the event loop (`FdWatch`),
 * so a slow program (a network request, a mixer that hangs) never blocks the
 * shell. The function gets the result when the program ends, or when the time
 * is up and the program (with the processes of its session) is killed.
 * Without a shell: the arguments are not interpreted.
 */
class Runner {
  public:
    using Done = std::function<void(const RunResult&)>;

    struct Options {
        std::chrono::milliseconds timeout{30000};
        size_t max_output = size_t{1024} * 1024; //!< bytes kept from the output
    };

    Runner(Timers& timers, FdWatch& watches);

    //! Kills the programs that still run; their functions are not called
    ~Runner();

    Runner(const Runner&) = delete;
    Runner& operator=(const Runner&) = delete;

    /**
     * @short Start `argv[0]` (searched in PATH).
     *
     * False, with errno set, when it can't be started; `done` is not called
     * then. Otherwise `done` is called later, from the loop.
     */
    bool run(const std::vector<std::string>& argv, Done done,
             const Options& options);
    bool run(const std::vector<std::string>& argv, Done done);

    //! The programs that run now
    size_t running() const { return jobs.size(); }

    //! Kill all of them, no function is called
    void cancel_all();

  private:
    struct Job {
        pid_t pid = -1;
        int fd = -1;
        FdWatch::Id watch = 0;
        Timers::Id timer = 0;
        bool eof = false;
        Options options;
        RunResult result;
        Done done;
    };

    void on_readable(unsigned long id);
    void on_timeout(unsigned long id);
    void finish(unsigned long id);
    void discard(Job& job);

    Timers& timers;
    FdWatch& watches;
    unsigned long next_id = 1;
    std::map<unsigned long, std::unique_ptr<Job>> jobs;
};

} // namespace tiny
