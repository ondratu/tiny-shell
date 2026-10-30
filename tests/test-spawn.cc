#include <gtest/gtest.h>

#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "spawn.h"

namespace {

template <class Done>
bool wait_for(Done done)
{
    for (int i = 0; i < 300; ++i) {
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

std::filesystem::path temp_file(const char* name)
{
    auto path = std::filesystem::temp_directory_path() /
                (std::string(name) + std::to_string(getpid()));
    std::filesystem::remove(path);
    return path;
}

} // namespace

TEST(Spawn, runs_the_command)
{
    const auto marker = temp_file("tiny-spawn-run-");
    EXPECT_GT(tiny::spawn_shell("touch " + marker.string()), 0);
    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
}

TEST(Spawn, child_leads_its_own_session)
{
    const auto out = temp_file("tiny-spawn-sid-");
    // ps differs between systems (busybox has no sid); a process group
    // with the id of the shell exists only if the shell leads its session
    const pid_t pid =
            tiny::spawn_shell("kill -0 -$$ && echo ok > " + out.string());
    ASSERT_GT(pid, 0);
    ASSERT_TRUE(wait_for([&] { return std::filesystem::exists(out); }));
    EXPECT_NE(getsid(0), pid);
    std::filesystem::remove(out);
}

TEST(Spawn, finished_children_are_reaped)
{
    tiny::reap_children();
    const pid_t pid = tiny::spawn({"true"});
    ASSERT_GT(pid, 0);
    // a reaped child is gone: waitpid has nothing to wait for
    EXPECT_TRUE(wait_for([&] { return waitpid(pid, nullptr, WNOHANG) < 0; }));
}

TEST(Spawn, shell_command_line_has_shell_features)
{
    const auto marker = temp_file("tiny-spawn-shell-");
    EXPECT_GT(tiny::spawn_shell("echo a | cat > " + marker.string()), 0);
    EXPECT_TRUE(wait_for([&] {
        return std::filesystem::exists(marker) &&
               std::filesystem::file_size(marker) > 0;
    }));
    std::filesystem::remove(marker);
}

TEST(Spawn, shell_runs_a_whole_command_line)
{
    const auto marker = temp_file("tiny-spawn-and-");
    EXPECT_GT(tiny::spawn_shell("true && touch " + marker.string()), 0);
    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
}

TEST(Spawn, arguments_are_passed_without_a_shell)
{
    const auto marker = temp_file("tiny-spawn a b;c-");
    EXPECT_GT(tiny::spawn({"touch", marker.string()}), 0);
    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
}

TEST(Spawn, unknown_program_is_reported)
{
    tiny::reap_children();
    errno = 0;
    EXPECT_EQ(tiny::spawn({"/nonexistent/program"}), -1);
    EXPECT_EQ(errno, ENOENT);
}

TEST(Spawn, empty_command_is_refused)
{
    errno = 0;
    EXPECT_EQ(tiny::spawn({}), -1);
    EXPECT_EQ(errno, EINVAL);
    EXPECT_EQ(tiny::spawn({""}), -1);
}

TEST(Spawn, unknown_shell_command_does_not_stop_the_caller)
{
    tiny::reap_children();
    EXPECT_GT(tiny::spawn_shell("/nonexistent/program"), 0);
}

namespace {

// Lowers a limit of the process to zero for the lifetime of the object.
class NoResource {
  public:
    explicit NoResource(int resource_arg): resource(resource_arg)
    {
        getrlimit(resource, &old);
        rlimit none = old;
        none.rlim_cur = 0;
        setrlimit(resource, &none);
    }

    ~NoResource() { setrlimit(resource, &old); }

  private:
    int resource;
    rlimit old = {};
};

} // namespace

TEST(Spawn, no_free_descriptor_is_reported)
{
    errno = 0;
    pid_t pid = 0;
    {
        const NoResource limit(RLIMIT_NOFILE);
        pid = tiny::spawn({"true"});
    }

    EXPECT_EQ(pid, -1);
    EXPECT_EQ(errno, EMFILE);
}

TEST(Spawn, failed_fork_is_reported)
{
    if (geteuid() == 0) {
        GTEST_SKIP() << "the limit of processes does not apply to root";
    }
    errno = 0;
    pid_t pid = 0;
    {
        const NoResource limit(RLIMIT_NPROC);
        pid = tiny::spawn({"true"});
    }

    EXPECT_EQ(pid, -1);
    EXPECT_EQ(errno, EAGAIN);
}

TEST(SpawnCommand, plain_words_are_started_without_a_shell)
{
    const auto marker = temp_file("tiny-spawn-cmd-");
    EXPECT_GT(tiny::spawn_command("  touch   " + marker.string() + " "), 0);
    EXPECT_TRUE(wait_for([&] { return std::filesystem::exists(marker); }));
    std::filesystem::remove(marker);
}

TEST(SpawnCommand, missing_program_is_reported)
{
    EXPECT_EQ(tiny::spawn_command("tiny-no-such-program --flag"), -1);
    EXPECT_EQ(errno, ENOENT);
    EXPECT_EQ(tiny::spawn_command("   "), -1);
}

TEST(SpawnCommand, shell_syntax_goes_to_the_shell)
{
    const auto marker = temp_file("tiny-spawn-cmd-sh-");
    EXPECT_GT(tiny::spawn_command("echo a | cat > " + marker.string()), 0);
    EXPECT_TRUE(wait_for([&] {
        return std::filesystem::exists(marker) &&
               std::filesystem::file_size(marker) > 0;
    }));
    std::filesystem::remove(marker);

    EXPECT_GT(tiny::spawn_command("FOO=1 true"), 0);
    EXPECT_GT(tiny::spawn_command("true \"quoted arg\""), 0);
}

TEST(Spawn, child_does_not_inherit_blocked_signals)
{
    const auto out = temp_file("tiny-spawn-mask-");
    sigset_t block;
    sigset_t before;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    sigprocmask(SIG_BLOCK, &block, &before);

    // the status of cp itself (a shell would clear the mask on its own)
    const pid_t pid = tiny::spawn({"cp", "/proc/self/status", out.string()});
    sigprocmask(SIG_SETMASK, &before, nullptr);

    ASSERT_GT(pid, 0);
    ASSERT_TRUE(wait_for([&] {
        return std::filesystem::exists(out) &&
               std::filesystem::file_size(out) > 0;
    }));
    std::ifstream in(out);
    std::string line;
    while (std::getline(in, line) && line.rfind("SigBlk:", 0) != 0) {}
    ASSERT_EQ(line.rfind("SigBlk:", 0), 0u) << "no SigBlk line";
    EXPECT_EQ(line.find_first_not_of("SigBlk: \t0"), std::string::npos) << line;
    std::filesystem::remove(out);
}

TEST(SpawnCommand, any_blank_separates_the_words)
{
    EXPECT_GT(tiny::spawn_command("true\r"), 0);
    EXPECT_GT(tiny::spawn_command("\vtrue\f"), 0);
}
