#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "app/lock_check.h"

using tiny::app::Checker;
using tiny::app::Verdict;

namespace {

//! A checker that is a shell script: the right password is "secret"
Checker script(const std::string& body, int wrong_status = 1,
               bool terminating_zero = false)
{
    Checker checker;
    checker.path = "/bin/sh";
    checker.arguments = {"-c", body};
    checker.wrong_status = wrong_status;
    checker.terminating_zero = terminating_zero;
    return checker;
}

const char* const secret_script = "read password; [ \"$password\" = secret ]";

} // namespace

TEST(LockCheckTest, the_right_password_is_right)
{
    EXPECT_EQ(tiny::app::run_checker(script(secret_script), "secret").verdict,
              Verdict::Right);
}

TEST(LockCheckTest, the_wrong_password_is_wrong)
{
    EXPECT_EQ(tiny::app::run_checker(script(secret_script), "other").verdict,
              Verdict::Wrong);
}

TEST(LockCheckTest, the_wrong_status_is_the_one_of_the_checker)
{
    const Checker seven = script("exit 7", 7);
    EXPECT_EQ(tiny::app::run_checker(seven, "x").verdict, Verdict::Wrong);
    const Checker one = script("exit 1", 7); // not the wrong status: a failure
    EXPECT_EQ(tiny::app::run_checker(one, "x").verdict, Verdict::Failed);
}

TEST(LockCheckTest, the_password_is_followed_by_a_zero_when_asked)
{
    const std::string body = "head -c 7 | od -An -c | grep -q '\\\\0'";
    EXPECT_EQ(tiny::app::run_checker(script(body, 1, true), "secret").verdict,
              Verdict::Right);
    EXPECT_EQ(tiny::app::run_checker(script(body, 1, false), "secret").verdict,
              Verdict::Wrong);
}

TEST(LockCheckTest, the_checker_has_no_environment)
{
    setenv("TINY_LOCK_TEST", "visible", 1);
    EXPECT_EQ(tiny::app::run_checker(script("[ -z \"$TINY_LOCK_TEST\" ]"), "")
                      .verdict,
              Verdict::Right);
    unsetenv("TINY_LOCK_TEST");
}

TEST(LockCheckTest, a_checker_that_does_not_read_is_not_a_crash)
{
    // the write to the closed pipe must not kill the test with SIGPIPE
    const std::string long_password(1 << 20, 'a');
    EXPECT_EQ(tiny::app::run_checker(script("exit 0"), long_password).verdict,
              Verdict::Right);
}

TEST(LockCheckTest, a_program_that_does_not_exist_fails)
{
    Checker checker;
    checker.path = "/nonexistent/checker";
    const auto outcome = tiny::app::run_checker(checker, "x");
    EXPECT_EQ(outcome.verdict, Verdict::Failed);
    EXPECT_NE(outcome.text.find("/nonexistent/checker"), std::string::npos);
}

TEST(LockCheckTest, no_program_fails)
{
    EXPECT_EQ(tiny::app::run_checker(Checker(), "x").verdict, Verdict::Failed);
}

TEST(LockCheckTest, a_killed_checker_fails)
{
    EXPECT_EQ(tiny::app::run_checker(script("kill -9 $$"), "x").verdict,
              Verdict::Failed);
}

TEST(LockCheckTest, another_status_fails_and_tells_it)
{
    const auto outcome = tiny::app::run_checker(script("exit 3"), "x");
    EXPECT_EQ(outcome.verdict, Verdict::Failed);
    EXPECT_NE(outcome.text.find('3'), std::string::npos);
}

TEST(LockCheckTest, wipe_overwrites_and_empties_the_text)
{
    std::string text = "secret";
    const char* const bytes = text.data();
    tiny::app::wipe(text);
    EXPECT_TRUE(text.empty());
    EXPECT_EQ(bytes[0], '\0'); // the buffer was overwritten, not just emptied
    EXPECT_EQ(bytes[5], '\0');
}

TEST(LockCheckTest, the_user_is_the_one_of_the_process)
{
    EXPECT_FALSE(tiny::app::user_name().empty());
}

TEST(LockCheckTest, unix_chkpwd_gets_the_user_and_nonull)
{
    const Checker checker = tiny::app::unix_chkpwd("/x/unix_chkpwd", "anna");
    EXPECT_EQ(checker.path, "/x/unix_chkpwd");
    EXPECT_EQ(checker.arguments, (std::vector<std::string>{"anna", "nonull"}));
    EXPECT_EQ(checker.wrong_status, 7);
    EXPECT_TRUE(checker.terminating_zero);
}

TEST(LockCheckTest, unix_chkpwd_is_searched_when_no_path_is_given)
{
    const Checker checker = tiny::app::unix_chkpwd("", "anna");
    // there is one on a Linux with PAM, none on a system without it
    EXPECT_TRUE(checker.path.empty() || tiny::app::can_run(checker.path));
}

TEST(LockCheckTest, tiny_pwcheck_gets_no_arguments)
{
    const Checker checker = tiny::app::tiny_pwcheck("/x/tiny-lock-pwcheck");
    EXPECT_EQ(checker.path, "/x/tiny-lock-pwcheck");
    EXPECT_TRUE(checker.arguments.empty());
    EXPECT_EQ(checker.wrong_status, 1);
    EXPECT_FALSE(checker.terminating_zero);
}

TEST(LockCheckTest, privileges_are_told_from_the_mode_of_the_file)
{
    const auto file = std::filesystem::temp_directory_path() /
                      ("tiny-lock-mode-" + std::to_string(getpid()));
    std::ofstream(file) << "#!/bin/sh\n";
    chmod(file.c_str(), 0755);
    EXPECT_TRUE(tiny::app::can_run(file));
    EXPECT_FALSE(tiny::app::has_privileges(file));
    chmod(file.c_str(), 02755);
    EXPECT_TRUE(tiny::app::has_privileges(file));
    chmod(file.c_str(), 04755);
    EXPECT_TRUE(tiny::app::has_privileges(file));
    chmod(file.c_str(), 0644);
    EXPECT_FALSE(tiny::app::can_run(file));
    EXPECT_FALSE(tiny::app::has_privileges(file));
    std::filesystem::remove(file);
    EXPECT_FALSE(tiny::app::can_run(file));
    EXPECT_FALSE(tiny::app::can_run(""));
}

TEST(LockCheckTest, the_whole_name_is_the_text_before_the_first_comma)
{
    EXPECT_EQ(tiny::app::full_name_from("Anna Nováková,Room 5,555,", "anna"),
              "Anna Nováková");
    EXPECT_EQ(tiny::app::full_name_from("  Anna Nováková  ", "anna"),
              "Anna Nováková");
}

TEST(LockCheckTest, without_a_whole_name_the_login_is_shown)
{
    EXPECT_EQ(tiny::app::full_name_from("", "anna"), "anna");
    EXPECT_EQ(tiny::app::full_name_from(",Room 5", "anna"), "anna");
    EXPECT_EQ(tiny::app::full_name_from("   ", "anna"), "anna");
}

TEST(LockCheckTest, the_user_has_a_name_to_show)
{
    EXPECT_FALSE(tiny::app::user_full_name().empty());
}
