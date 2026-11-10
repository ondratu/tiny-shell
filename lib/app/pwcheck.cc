#include "app/pwcheck.h"

#include <pwd.h>
#include <sys/resource.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <ctime>

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#if defined(__has_include)
#if __has_include(<shadow.h>)
#include <shadow.h>
#define TINY_HAVE_SHADOW 1
#endif
#if __has_include(<crypt.h>)
#include <crypt.h>
#endif
#endif

namespace tiny::app {

namespace {

constexpr size_t max_password = 1024;

enum Status { Right = 0, Wrong = 1, CantCheck = 2 };

//! Same time for the same length, whatever the first difference is
bool same_text(const char* a, const char* b)
{
    const size_t length = strlen(a);
    if (length != strlen(b)) {
        return false;
    }
    unsigned char different = 0;
    for (size_t i = 0; i < length; ++i) {
        different |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return different == 0;
}

//! One line of the input, without the end of the line
std::string read_line(int input)
{
    std::string line;
    while (line.size() < max_password) {
        char c;
        const ssize_t got = read(input, &c, 1);
        if (got < 0 && errno == EINTR) {
            continue;
        }
        if (got != 1 || c == '\n' || c == '\0') {
            break;
        }
        line += c;
    }
    return line;
}

void sleep_ms(unsigned ms)
{
    timespec wait = {static_cast<time_t>(ms / 1000),
                     static_cast<long>(ms % 1000) * 1000000L};
    while (nanosleep(&wait, &wait) != 0 && errno == EINTR) {}
}

} // namespace

std::string password_hash(const std::string& user)
{
#ifdef TINY_HAVE_SHADOW
    if (const spwd* shadow = getspnam(user.c_str());
        shadow != nullptr && shadow->sp_pwdp != nullptr) {
        return shadow->sp_pwdp;
    }
#endif
    // BSD: the hash is in the passwd entry, and only the user 0 sees it
    if (const passwd* entry = getpwnam(user.c_str());
        entry != nullptr && entry->pw_passwd != nullptr) {
        return entry->pw_passwd;
    }
    return "";
}

bool hash_usable(const std::string& hash)
{
    return !hash.empty() && hash[0] != '!' && hash[0] != '*' && hash != "x";
}

Verdict verify_password(const std::string& hash, const std::string& password)
{
    if (!hash_usable(hash)) {
        return Verdict::Failed;
    }
    if (password.empty()) {
        return Verdict::Wrong;
    }
    const char* made = crypt(password.c_str(), hash.c_str());
    // an unknown format: glibc gives a text that starts with `*`
    if (made == nullptr || made[0] == '*') {
        return Verdict::Failed;
    }
    return same_text(made, hash.c_str()) ? Verdict::Right : Verdict::Wrong;
}

bool drop_privileges()
{
    const gid_t group = getgid();
    const uid_t user = getuid();
#if defined(__linux__)
    // sets the real, the effective and the saved id: it can't be undone
    if (setresgid(group, group, group) != 0 ||
        setresuid(user, user, user) != 0) {
        return false;
    }
#else
    if (setgid(group) != 0 || setuid(user) != 0) {
        return false;
    }
#endif
    if (getegid() != group || geteuid() != user) {
        return false;
    }
#if defined(__linux__)
    gid_t real_group;
    gid_t effective_group;
    gid_t saved_group;
    return getresgid(&real_group, &effective_group, &saved_group) == 0 &&
           saved_group == group;
#else
    return true;
#endif
}

void protect_memory()
{
    const rlimit none = {0, 0};
    setrlimit(RLIMIT_CORE, &none);
#if defined(__linux__)
    prctl(PR_SET_DUMPABLE, 0);
#endif
}

int run_pwcheck(int argc, char* argv[], int input, unsigned delay_ms)
{
    const bool probe = argc == 2 && strcmp(argv[1], "--probe") == 0;
    if (argc != 1 && !probe) {
        return CantCheck; // no arguments: nothing for a caller to steer
    }
    protect_memory();

    // The user is the one that runs it. Not an argument, not $USER.
    std::string hash = password_hash(user_name());
    if (!drop_privileges()) { // the hash is read, the privileges are not needed
        wipe(hash);
        return CantCheck;
    }
    if (!hash_usable(hash)) {
        wipe(hash);
        return CantCheck;
    }
    if (probe) {
        wipe(hash);
        return Right;
    }
    std::string password = read_line(input);
    const Verdict verdict = verify_password(hash, password);
    wipe(password);
    wipe(hash);
    switch (verdict) {
        case Verdict::Right:
            return Right;
        case Verdict::Wrong:
            sleep_ms(delay_ms);
            return Wrong;
        default:
            return CantCheck;
    }
}

} // namespace tiny::app
