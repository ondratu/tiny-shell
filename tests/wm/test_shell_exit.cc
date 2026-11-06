#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include "display.h"
#include "wm/selection.h"

using namespace std::chrono_literals;

namespace {

//! The tiny-shell built next to the tests: build/tests/wm/<test> is two
//! levels under build/. Empty when this system can't tell where the test is.
std::string shell_program()
{
    char path[4096];
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (length <= 0) {
        return "";
    }
    std::string dir(path, static_cast<size_t>(length));
    for (int i = 0; i < 3; ++i) { // the test, wm/, tests/
        dir.erase(dir.rfind('/'));
    }
    return dir + "/tiny-shell";
}

bool wm_runs()
{
    ::Display* display = tiny::get_display();
    const Atom wm = XInternAtom(
            display, tiny::wm::selection_name(XDefaultScreen(display)).c_str(),
            False);
    return XGetSelectionOwner(display, wm) != None;
}

//! Start tiny-shell on the display of the tests with no configuration of the
//! user, wait for it to take the screen
pid_t start_shell(const std::string& program)
{
    const pid_t pid = fork();
    if (pid == 0) {
        setenv("XDG_CONFIG_HOME", "/nonexistent", 1);
        setenv("XDG_CONFIG_DIRS", "/nonexistent", 1);
        setenv("HOME", "/nonexistent", 1);
        execl(program.c_str(), program.c_str(), static_cast<char*>(nullptr));
        _exit(127);
    }
    for (int i = 0; i < 200 && pid > 0; ++i) {
        int status = 0;
        XSync(tiny::get_display(), False);
        if (waitpid(pid, &status, WNOHANG) == pid) {
            return -1; // it did not start
        }
        if (wm_runs()) {
            return pid;
        }
        std::this_thread::sleep_for(50ms);
    }
    if (pid > 0) {
        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }
    return -1;
}

} // namespace

class ShellExitTest : public ::testing::TestWithParam<int> {};

// The fonts of the styles were closed after the display: a crash at the exit
TEST_P(ShellExitTest, a_signal_ends_the_shell_cleanly)
{
    const std::string program = shell_program();
    if (access(program.c_str(), X_OK) != 0) {
        GTEST_SKIP() << "no " << program;
    }
    if (wm_runs()) {
        GTEST_SKIP() << "another window manager runs on this display";
    }
    const pid_t pid = start_shell(program);
    ASSERT_GT(pid, 0) << "tiny-shell did not start";

    kill(pid, GetParam());

    int status = 0;
    ASSERT_EQ(waitpid(pid, &status, 0), pid);
    ASSERT_TRUE(WIFEXITED(status)) << "killed by signal " << WTERMSIG(status);
    EXPECT_EQ(WEXITSTATUS(status), EXIT_SUCCESS);
}

INSTANTIATE_TEST_SUITE_P(Signals, ShellExitTest,
                         ::testing::Values(SIGTERM, SIGINT));
