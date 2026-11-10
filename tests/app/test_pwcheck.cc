#include <gtest/gtest.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cstring>

#include "app/pwcheck.h"

#if defined(__has_include)
#if __has_include(<crypt.h>)
#include <crypt.h>
#endif
#endif

using tiny::app::Verdict;

namespace {

//! A hash made by this system, "" if it knows none of the formats
std::string hash_of(const std::string& password)
{
    for (const char* salt : {"$6$tinysalt$", "$5$tinysalt$", "$1$tinysalt$",
                             "$2b$04$tinysaltvalue0123456u", "ab"}) {
        const char* made = crypt(password.c_str(), salt);
        if (made != nullptr && made[0] != '*') {
            return made;
        }
    }
    return "";
}

//! What run_pwcheck makes of the arguments and of a password on the input
int run(std::vector<const char*> arguments, const std::string& input)
{
    int pipes[2];
    EXPECT_EQ(pipe(pipes), 0);
    EXPECT_EQ(write(pipes[1], input.data(), input.size()),
              static_cast<ssize_t>(input.size()));
    close(pipes[1]);
    arguments.insert(arguments.begin(), "tiny-lock-pwcheck");
    const int status = tiny::app::run_pwcheck(
            static_cast<int>(arguments.size()),
            const_cast<char**>(arguments.data()), pipes[0], 0);
    close(pipes[0]);
    return status;
}

} // namespace

TEST(PwcheckTest, the_right_password_matches_its_hash)
{
    const std::string hash = hash_of("secret");
    if (hash.empty()) {
        GTEST_SKIP() << "crypt() knows no format that the test tries";
    }
    EXPECT_EQ(tiny::app::verify_password(hash, "secret"), Verdict::Right);
}

TEST(PwcheckTest, another_password_does_not_match)
{
    const std::string hash = hash_of("secret");
    if (hash.empty()) {
        GTEST_SKIP() << "crypt() knows no format that the test tries";
    }
    EXPECT_EQ(tiny::app::verify_password(hash, "Secret"), Verdict::Wrong);
    EXPECT_EQ(tiny::app::verify_password(hash, "secret "), Verdict::Wrong);
    EXPECT_EQ(tiny::app::verify_password(hash, "secre"), Verdict::Wrong);
}

TEST(PwcheckTest, an_empty_password_is_wrong)
{
    const std::string hash = hash_of("secret");
    if (hash.empty()) {
        GTEST_SKIP() << "crypt() knows no format that the test tries";
    }
    EXPECT_EQ(tiny::app::verify_password(hash, ""), Verdict::Wrong);
}

TEST(PwcheckTest, an_empty_hash_is_not_an_account_without_a_password)
{
    // such an account would be open to anybody: the lock refuses it instead
    EXPECT_EQ(tiny::app::verify_password("", ""), Verdict::Failed);
    EXPECT_EQ(tiny::app::verify_password("", "x"), Verdict::Failed);
}

TEST(PwcheckTest, a_locked_account_is_not_checked)
{
    EXPECT_EQ(tiny::app::verify_password("!", "x"), Verdict::Failed);
    EXPECT_EQ(tiny::app::verify_password("*", "x"), Verdict::Failed);
    EXPECT_EQ(tiny::app::verify_password("!$6$salt$hash", "x"),
              Verdict::Failed);
    EXPECT_EQ(tiny::app::verify_password("x", "x"), Verdict::Failed);
}

TEST(PwcheckTest, a_hash_in_an_unknown_format_fails)
{
    EXPECT_EQ(tiny::app::verify_password("$99$what$ever", "x"),
              Verdict::Failed);
}

TEST(PwcheckTest, only_a_real_hash_is_usable)
{
    EXPECT_TRUE(tiny::app::hash_usable("$6$salt$hash"));
    EXPECT_FALSE(tiny::app::hash_usable(""));
    EXPECT_FALSE(tiny::app::hash_usable("!"));
    EXPECT_FALSE(tiny::app::hash_usable("!!"));
    EXPECT_FALSE(tiny::app::hash_usable("*"));
    EXPECT_FALSE(tiny::app::hash_usable("x"));
}

TEST(PwcheckTest, a_user_that_does_not_exist_has_no_hash)
{
    EXPECT_EQ(tiny::app::password_hash("no-such-user-tiny-lock"), "");
}

TEST(PwcheckTest, a_program_that_has_nothing_to_drop_can_drop_it)
{
    EXPECT_TRUE(tiny::app::drop_privileges());
}

TEST(PwcheckTest, memory_is_protected_from_core_dumps)
{
    rlimit before;
    getrlimit(RLIMIT_CORE, &before);
    tiny::app::protect_memory();
    rlimit after;
    ASSERT_EQ(getrlimit(RLIMIT_CORE, &after), 0);
    EXPECT_EQ(after.rlim_cur, 0u);
    EXPECT_EQ(after.rlim_max, 0u);
}

TEST(PwcheckTest, arguments_are_refused)
{
    // nothing a caller passes can name another user or change the check
    EXPECT_EQ(run({"root"}, "x\n"), 2);
    EXPECT_EQ(run({"--probe", "x"}, ""), 2);
    EXPECT_EQ(run({"--user=root"}, "x\n"), 2);
}

TEST(PwcheckTest, a_password_never_gets_the_answer_right_by_chance)
{
    // The hash of the user that runs the test can't be read without the
    // privileges (or is not there in a container): 2. When it can be read,
    // this password is wrong: 1. It is never 0.
    const int status = run({}, "tiny-lock-test-password-no-one-has\n");
    EXPECT_TRUE(status == 1 || status == 2) << status;
}

TEST(PwcheckTest, probe_does_not_read_the_password)
{
    const int status = run({"--probe"}, "");
    EXPECT_TRUE(status == 0 || status == 2) << status;
}
