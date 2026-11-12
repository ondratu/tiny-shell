#include "helpers.h"

#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "spawn.h"

// src/tiny-shell-session runs in the root of the repository, with a fake
// tiny-shell in PATH that prints what it was given and what it sees.

namespace fs = std::filesystem;

namespace {

class SessionScriptTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-session" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "bin");
        fs::create_directories(root / "home");
        const fs::path fake = root / "bin" / "tiny-shell";
        std::ofstream(fake) << "#!/bin/sh\n"
                               "exec > \"$TINY_TEST_OUT\"\n"
                               "echo \"args=$*\"\n"
                               "echo \"desktop=$XDG_CURRENT_DESKTOP\"\n"
                               "echo \"session=$XDG_SESSION_DESKTOP\"\n"
                               "echo \"data=$XDG_DATA_DIRS\"\n"
                               "echo \"path=$PATH\"\n"
                               "echo \"mine=$FROM_THE_USER\"\n";
        chmod(fake.c_str(), 0755);
    }

    void TearDown() override { fs::remove_all(root); }

    //! What the fake tiny-shell wrote. `env` is "NAME=value" to add to the
    //! environment; the variables of the session that the test cares for are
    //! removed first, and the bus is declared so that no dbus-run-session
    //! starts.
    std::string run(const std::vector<std::string>& env = {},
                    const std::vector<std::string>& args = {})
    {
        const fs::path out = root / "out";
        std::vector<std::string> environment = {
                "HOME=" + (root / "home").string(),
                "PATH=" + (root / "bin").string() + ":/usr/bin:/bin",
                "DBUS_SESSION_BUS_ADDRESS=none",
                "TINY_TEST_OUT=" + out.string(),
                "XDG_CURRENT_DESKTOP",
                "XDG_SESSION_DESKTOP",
                "XDG_DATA_DIRS",
                "XDG_CONFIG_HOME"};
        environment.insert(environment.end(), env.begin(), env.end());
        std::vector<std::string> argv = {"/bin/sh", "src/tiny-shell-session"};
        argv.insert(argv.end(), args.begin(), args.end());

        const pid_t pid = tiny::spawn(argv, "", environment);
        EXPECT_GT(pid, 0);
        int status = 0;
        for (int i = 0; i < 500 && !tiny::child_ended(pid, &status); ++i) {
            tiny::reap_children();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::ifstream file(out);
        return {std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()};
    }

    fs::path root;
};

} // namespace

TEST_F(SessionScriptTest, the_window_manager_gets_the_arguments_of_the_session)
{
    EXPECT_NE(run({}, {"--replace"}).find("args=--replace\n"),
              std::string::npos);
}

TEST_F(SessionScriptTest, the_desktop_is_named_when_the_system_does_not)
{
    const std::string out = run();

    EXPECT_NE(out.find("desktop=tiny-shell\n"), std::string::npos) << out;
    EXPECT_NE(out.find("session=tiny-shell\n"), std::string::npos) << out;
}

TEST_F(SessionScriptTest, a_desktop_that_a_display_manager_set_stays)
{
    const std::string out = run({"XDG_CURRENT_DESKTOP=TINY:other"});

    EXPECT_NE(out.find("desktop=TINY:other\n"), std::string::npos) << out;
}

TEST_F(SessionScriptTest, the_programs_of_the_user_are_found)
{
    const std::string out = run();

    EXPECT_NE(out.find("path=" + (root / "home").string() + "/.local/bin:"),
              std::string::npos)
            << out;
}

TEST_F(SessionScriptTest, the_data_directories_of_the_packages_are_named)
{
    const std::string out = run();

    EXPECT_NE(out.find("data="), std::string::npos) << out;
    EXPECT_NE(out.find("/usr/share"), std::string::npos) << out;
}

TEST_F(SessionScriptTest, the_environment_file_of_the_user_is_read)
{
    fs::create_directories(root / "home" / ".config" / "tiny-shell");
    std::ofstream(root / "home" / ".config" / "tiny-shell" / "environment")
            << "export FROM_THE_USER=yes\n";

    EXPECT_NE(run().find("mine=yes\n"), std::string::npos);
}

TEST_F(SessionScriptTest, the_environment_file_is_found_in_xdg_config_home)
{
    fs::create_directories(root / "cfg" / "tiny-shell");
    std::ofstream(root / "cfg" / "tiny-shell" / "environment")
            << "export FROM_THE_USER=xdg\n";

    EXPECT_NE(run({"XDG_CONFIG_HOME=" + (root / "cfg").string()})
                      .find("mine=xdg\n"),
              std::string::npos);
}
