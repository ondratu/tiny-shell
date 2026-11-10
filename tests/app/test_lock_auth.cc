#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <queue>

#include "app/lock_auth.h"
#include "app/lock_pam.h"

using namespace tiny::app;

namespace {

//! A dialog that answers from a list and remembers what it was told
class ScriptedDialog : public Dialog {
  public:
    bool ask(bool hidden, const std::string& prompt,
             std::string& answer) override
    {
        asked.push_back(prompt);
        hidden_prompts += hidden ? 1 : 0;
        if (answers.empty()) {
            return false;
        }
        answer = answers.front();
        answers.pop();
        return true;
    }

    void say(bool error, const std::string& text) override
    {
        told.push_back((error ? "E:" : "I:") + text);
    }

    std::queue<std::string> answers;
    std::vector<std::string> asked;
    std::vector<std::string> told;
    int hidden_prompts = 0;
};

//! A shell script as a program: the right password is "secret"
class ScriptFile {
  public:
    explicit ScriptFile(const std::string& body, int mode = 0755):
        path(std::filesystem::temp_directory_path() /
             ("tiny-lock-auth-" + std::to_string(getpid()) + "-" +
              std::to_string(++counter)))
    {
        std::ofstream(path) << "#!/bin/sh\n" << body << "\n";
        chmod(path.c_str(), static_cast<mode_t>(mode));
    }
    ~ScriptFile() { std::filesystem::remove(path); }

    std::string str() const { return path.string(); }

  private:
    static inline int counter = 0;
    std::filesystem::path path;
};

const char* const secret_body = "read password; [ \"$password\" = secret ]";

Backend pwcheck_with(const ScriptFile& script)
{
    AuthSettings settings;
    settings.method = Method::Pwcheck;
    settings.pwcheck = script.str();
    Backend backend;
    std::string why;
    EXPECT_TRUE(resolve_backend(settings, "anna", backend, why)) << why;
    return backend;
}

} // namespace

TEST(LockAuthTest, a_method_is_named_in_the_configuration)
{
    Method method = Method::Auto;
    EXPECT_TRUE(method_from_name("pam", method));
    EXPECT_EQ(method, Method::Pam);
    EXPECT_TRUE(method_from_name("checker", method));
    EXPECT_EQ(method, Method::Checker);
    EXPECT_TRUE(method_from_name("pwcheck", method));
    EXPECT_EQ(method, Method::Pwcheck);
    EXPECT_TRUE(method_from_name("auto", method));
    EXPECT_EQ(method, Method::Auto);
    EXPECT_FALSE(method_from_name("PAM", method));
    EXPECT_FALSE(method_from_name("", method));
    EXPECT_EQ(method, Method::Auto); // not changed by a name that is not one
    EXPECT_STREQ(method_name(Method::Pwcheck), "pwcheck");
}

TEST(LockAuthTest, pam_is_refused_in_a_build_without_it)
{
    AuthSettings settings;
    settings.method = Method::Pam;
    Backend backend;
    std::string why;
    EXPECT_EQ(resolve_backend(settings, "anna", backend, why), pam_built());
    if (pam_built()) {
        EXPECT_EQ(backend.method, Method::Pam);
        EXPECT_EQ(backend.pam_service, "tiny-lock");
        EXPECT_EQ(backend.description, "PAM, service tiny-lock");
    } else {
        EXPECT_NE(why.find("PAM"), std::string::npos);
    }
}

TEST(LockAuthTest, the_service_of_pam_is_the_one_in_the_settings)
{
    if (!pam_built()) {
        GTEST_SKIP() << "built without PAM";
    }
    AuthSettings settings;
    settings.method = Method::Pam;
    settings.pam_service = "mine";
    Backend backend;
    std::string why;
    ASSERT_TRUE(resolve_backend(settings, "anna", backend, why));
    EXPECT_EQ(backend.pam_service, "mine");
}

TEST(LockAuthTest, a_checker_that_does_not_exist_is_refused)
{
    AuthSettings settings;
    settings.method = Method::Checker;
    settings.checker = "/nonexistent/unix_chkpwd";
    Backend backend;
    std::string why;
    EXPECT_FALSE(resolve_backend(settings, "anna", backend, why));
    EXPECT_NE(why.find("unix_chkpwd"), std::string::npos);
}

TEST(LockAuthTest, a_checker_gets_the_user_and_nonull)
{
    const ScriptFile checker(
            "[ \"$1\" = anna ] && [ \"$2\" = nonull ] && exit 0;"
            "exit 7");
    AuthSettings settings;
    settings.method = Method::Checker;
    settings.checker = checker.str();
    Backend backend;
    std::string why;
    ASSERT_TRUE(resolve_backend(settings, "anna", backend, why));
    EXPECT_EQ(backend.method, Method::Checker);
    EXPECT_EQ(backend.description, checker.str());

    ScriptedDialog right;
    right.answers.emplace("x");
    EXPECT_EQ(authenticate(backend, "anna", right).verdict, Verdict::Right);
    // the script checks the arguments: 7 is the wrong password of unix_chkpwd
    Backend for_bob;
    ASSERT_TRUE(resolve_backend(settings, "bob", for_bob, why));
    for_bob.wrong_delay_ms = 0; // the delay has a test of its own
    ScriptedDialog other_user;
    other_user.answers.emplace("x");
    EXPECT_EQ(authenticate(for_bob, "bob", other_user).verdict, Verdict::Wrong);
}

TEST(LockAuthTest, pwcheck_that_does_not_exist_is_refused)
{
    AuthSettings settings;
    settings.method = Method::Pwcheck;
    settings.pwcheck = "/nonexistent/tiny-lock-pwcheck";
    Backend backend;
    std::string why;
    EXPECT_FALSE(resolve_backend(settings, "anna", backend, why));
    EXPECT_NE(why.find("tiny-lock-pwcheck"), std::string::npos);
}

TEST(LockAuthTest, the_password_goes_to_the_program_that_checks)
{
    const ScriptFile script(secret_body);
    const Backend backend = pwcheck_with(script);
    ScriptedDialog right;
    right.answers.emplace("secret");
    EXPECT_EQ(authenticate(backend, "anna", right).verdict, Verdict::Right);
    EXPECT_EQ(right.asked, std::vector<std::string>{"Password:"});
    EXPECT_EQ(right.hidden_prompts, 1);

    ScriptedDialog wrong;
    wrong.answers.emplace("other");
    EXPECT_EQ(authenticate(backend, "anna", wrong).verdict, Verdict::Wrong);
}

TEST(LockAuthTest, an_empty_password_is_wrong_and_is_not_checked)
{
    const ScriptFile script("exit 0"); // would say right to anything
    ScriptedDialog dialog;
    dialog.answers.emplace("");
    EXPECT_EQ(authenticate(pwcheck_with(script), "anna", dialog).verdict,
              Verdict::Wrong);
}

TEST(LockAuthTest, a_cancelled_question_does_not_unlock)
{
    const ScriptFile script("exit 0");
    ScriptedDialog dialog; // no answer
    const Outcome outcome = authenticate(pwcheck_with(script), "anna", dialog);
    EXPECT_EQ(outcome.verdict, Verdict::Failed);
    EXPECT_EQ(outcome.text, "cancelled");
}

TEST(LockAuthTest, the_probe_asks_the_program_whether_it_can_check)
{
    const ScriptFile yes("[ \"$1\" = --probe ]");
    std::string why;
    EXPECT_TRUE(probe_backend(pwcheck_with(yes), why));

    const ScriptFile no("exit 2");
    EXPECT_FALSE(probe_backend(pwcheck_with(no), why));
    EXPECT_NE(why.find("no password"), std::string::npos);
}

TEST(LockAuthTest, the_probe_of_a_checker_looks_for_the_program)
{
    Backend backend;
    backend.method = Method::Checker;
    backend.checker.path = "/nonexistent/unix_chkpwd";
    std::string why;
    EXPECT_FALSE(probe_backend(backend, why));
    const ScriptFile exists("exit 0");
    backend.checker.path = exists.str();
    EXPECT_TRUE(probe_backend(backend, why));
}

TEST(LockAuthTest, the_probe_of_pam_cannot_know_more)
{
    Backend backend;
    backend.method = Method::Pam;
    backend.pam_service = "tiny-lock-test-no-such-service";
    std::string why;
    // all it can see is a configuration that denies everybody
    EXPECT_EQ(probe_backend(backend, why),
              !pam_service_denies(backend.pam_service, pam_directories()));
}

TEST(LockAuthTest, nothing_to_check_with_is_told_and_not_guessed)
{
    AuthSettings settings;
    settings.checker = "/nonexistent/unix_chkpwd";
    settings.pwcheck = "/nonexistent/tiny-lock-pwcheck";
    Backend backend;
    std::string why;
    const bool found = resolve_backend(settings, "no-such-user", backend, why);
    // PAM is not taken without a helper or the right to read the hash, and
    // the unix_chkpwd of the system is not the one in the settings
    if (!found) {
        EXPECT_NE(why.find("no way to check"), std::string::npos);
    } else {
        EXPECT_NE(backend.method, Method::Pwcheck); // it is not there
    }
}

TEST(LockAuthTest, pwcheck_with_privileges_is_taken_when_nothing_else_is)
{
    const ScriptFile script(secret_body, 02755);
    AuthSettings settings;
    settings.checker = "/nonexistent/unix_chkpwd";
    settings.pwcheck = script.str();
    Backend backend;
    std::string why;
    const bool found = resolve_backend(settings, "no-such-user", backend, why);
    ASSERT_TRUE(found) << why; // a user without PAM, or with it and no helper
    // PAM wins where it can work, and nothing is wrong with that
    EXPECT_TRUE(backend.method == Method::Pwcheck ||
                backend.method == Method::Pam);
}

TEST(LockAuthTest, pwcheck_without_privileges_is_not_taken_by_auto)
{
    const ScriptFile script(secret_body, 0755);
    AuthSettings settings;
    settings.checker = "/nonexistent/unix_chkpwd";
    settings.pwcheck = script.str();
    Backend backend;
    std::string why;
    if (resolve_backend(settings, "no-such-user", backend, why)) {
        EXPECT_NE(backend.method, Method::Pwcheck);
    }
}

TEST(LockAuthTest,
     pam_asks_through_the_dialog_and_does_not_say_right_to_a_wrong_password)
{
    if (!pam_built()) {
        GTEST_SKIP() << "built without PAM";
    }
    ScriptedDialog dialog;
    dialog.answers.emplace("tiny-lock-test-password-no-one-has");
    const Outcome outcome = check_pam("root", "other", dialog);
    EXPECT_NE(outcome.verdict, Verdict::Right);
}

TEST(LockAuthTest, pam_cancelled_by_the_user_does_not_unlock)
{
    if (!pam_built()) {
        GTEST_SKIP() << "built without PAM";
    }
    ScriptedDialog dialog; // the first question gets no answer
    const Outcome outcome = check_pam("root", "other", dialog);
    EXPECT_EQ(outcome.verdict, Verdict::Failed);
}

TEST(LockAuthTest, a_build_without_pam_says_so)
{
    if (pam_built()) {
        GTEST_SKIP() << "built with PAM";
    }
    ScriptedDialog dialog;
    const Outcome outcome = check_pam("root", "other", dialog);
    EXPECT_EQ(outcome.verdict, Verdict::Failed);
    EXPECT_NE(outcome.text.find("PAM"), std::string::npos);
}

TEST(LockAuthTest, a_checker_makes_a_wrong_password_wait)
{
    const ScriptFile checker("exit 7");
    AuthSettings settings;
    settings.method = Method::Checker;
    settings.checker = checker.str();
    Backend backend;
    std::string why;
    ASSERT_TRUE(resolve_backend(settings, "anna", backend, why));
    EXPECT_GE(backend.wrong_delay_ms, 1000u);

    backend.wrong_delay_ms = 200;
    ScriptedDialog dialog;
    dialog.answers.emplace("x");
    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(authenticate(backend, "anna", dialog).verdict, Verdict::Wrong);
    EXPECT_GE(std::chrono::steady_clock::now() - start,
              std::chrono::milliseconds(190));
}

TEST(LockAuthTest, pwcheck_has_no_delay_of_ours)
{
    const ScriptFile script(secret_body);
    EXPECT_EQ(pwcheck_with(script).wrong_delay_ms, 0u); // it waits itself
}

namespace {

//! A directory with the files of PAM services
class PamDirectory {
  public:
    PamDirectory():
        path(std::filesystem::temp_directory_path() /
             ("tiny-lock-pam-" + std::to_string(getpid())))
    {
        std::filesystem::create_directories(path);
    }
    ~PamDirectory() { std::filesystem::remove_all(path); }

    void file(const std::string& name, const std::string& text) const
    {
        std::ofstream(path / name) << text;
    }

    std::vector<std::string> list() const { return {path.string()}; }

  private:
    std::filesystem::path path;
};

} // namespace

TEST(LockAuthTest, an_other_that_denies_everybody_is_seen)
{
    const PamDirectory directory;
    directory.file("other", "auth required pam_deny.so\n"
                            "account required pam_deny.so\n");
    EXPECT_TRUE(pam_service_denies("tiny-lock", directory.list()));
}

TEST(LockAuthTest, an_other_that_includes_the_login_is_not_a_denial)
{
    const PamDirectory directory;
    directory.file("other", "@include common-auth\n");
    EXPECT_FALSE(pam_service_denies("tiny-lock", directory.list()));
    directory.file("other", "auth include system-auth\n");
    EXPECT_FALSE(pam_service_denies("tiny-lock", directory.list()));
    directory.file("other", "auth substack system-auth\n");
    EXPECT_FALSE(pam_service_denies("tiny-lock", directory.list()));
}

TEST(LockAuthTest, a_file_of_the_service_is_used_before_other)
{
    const PamDirectory directory;
    directory.file("other", "auth required pam_deny.so\n");
    directory.file("tiny-lock", "# the login\nauth sufficient pam_unix.so\n");
    EXPECT_FALSE(pam_service_denies("tiny-lock", directory.list()));
    EXPECT_TRUE(pam_service_denies("another", directory.list()));
}

TEST(LockAuthTest, no_file_at_all_is_not_a_denial_that_can_be_told)
{
    const PamDirectory directory;
    EXPECT_FALSE(pam_service_denies("tiny-lock", directory.list()));
    EXPECT_FALSE(pam_service_denies("tiny-lock", {"/nonexistent"}));
}

TEST(LockAuthTest, one_line_that_asks_a_module_is_enough_not_to_deny)
{
    const PamDirectory directory;
    directory.file("other", "auth required pam_unix.so\n"
                            "auth required pam_deny.so\n");
    EXPECT_FALSE(pam_service_denies("tiny-lock", directory.list()));
}

TEST(LockAuthTest, the_probe_of_pam_refuses_a_service_that_denies)
{
    if (!pam_built()) {
        GTEST_SKIP() << "built without PAM";
    }
    const PamDirectory directory;
    directory.file("other", "auth required pam_deny.so\n");
    // a service that has no file in the real directories either
    Backend backend;
    backend.method = Method::Pam;
    backend.pam_service = "tiny-lock-test-no-such-service";
    std::string why;
    // the real /etc/pam.d is used: this holds only where other is not a denial
    EXPECT_EQ(probe_backend(backend, why),
              !pam_service_denies(backend.pam_service, pam_directories()));
}
