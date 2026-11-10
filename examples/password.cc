// Asks for the password of the user and says if it is right or wrong.
//
// An example of the password entry (Entry::set_password) and of checking a
// password the way a screen locker has to do it. Nothing is locked here.
//
// The password is checked by one of two backends:
//
//  - pam: PAM (pam_authenticate), so whatever the system has set up works: a
//    fingerprint, Howdy, a token. It is built when the make variable PAM is
//    not "no" and libpam is found (`make PAM=yes examples` insists on it).
//  - helper: a program that checks the password of the user that runs it and
//    reads it from its standard input. The default is `unix_chkpwd` of
//    Linux-PAM (the helper of pam_unix, set-group-ID shadow). It is the way
//    without libpam, and without the right to read /etc/shadow here. Its
//    path is searched for, or set with -H, or at the build time with
//    `make UNIX_CHKPWD=/path examples`.
//
// The check runs in a child process, so the window stays alive while PAM
// waits (a fingerprint, the delay after a wrong password). The password is
// overwritten in the entry as soon as the child is started.
//
//   password [-c] [-b pam|helper] [-s PAM-service] [-H helper] [-h]
//
// -c asks on the terminal (without echo) and checks the password once, with no
// window and no X: for a try over ssh. The exit status is 0 for the right
// password, 1 for the wrong one and 2 when it could not be checked.
//
// The PAM service is "tiny-example" and has no file in /etc/pam.d, so the
// system uses its "other" (Debian and NetBSD send it to the login password;
// elsewhere "other" may deny everything: choose a service with -s).
#include <X11/Xlib.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <sys/types.h>
#include <termios.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#ifdef TINY_HAVE_PAM
#include <security/pam_appl.h>
#endif

#include "container.h"
#include "entry.h"
#include "label.h"
#include "theme.h"
#include "x_util.h"

namespace tiny {
extern Handlers handlers;
}

namespace {

#ifndef TINY_UNIX_CHKPWD
#define TINY_UNIX_CHKPWD ""
#endif

// What unix_chkpwd returns for a wrong password: PAM_AUTH_ERR of Linux-PAM
constexpr int helper_wrong_password = 7;

enum class Verdict : char { Right = 'R', Wrong = 'W', Failed = 'F' };

struct Outcome {
    Verdict verdict;
    std::string text;
};

struct Backend {
    bool use_pam = false;
    std::string service = "tiny-example";
    std::string helper;
};

std::string user_name()
{
    // not $USER: the environment can say anything
    const passwd* entry = getpwuid(getuid());
    return entry != nullptr ? entry->pw_name : "";
}

#ifdef TINY_HAVE_PAM

struct Talk {
    const std::string* password;
    bool answered = false;
    std::string said; //!< the information and the error messages of the modules
};

// The answer to PAM: the first question gets the password, the information is
// kept to be shown. A second question (a code, ...) is not supported here, a
// locker would have to ask it.
int talk(int count, const pam_message** messages, pam_response** replies,
         void* data)
{
    auto* talk = static_cast<Talk*>(data);
    auto* answers = static_cast<pam_response*>(
            calloc(static_cast<size_t>(count), sizeof(pam_response)));
    if (answers == nullptr) {
        return PAM_BUF_ERR;
    }
    for (int i = 0; i < count; ++i) {
        const pam_message& message = *messages[i];
        if (message.msg_style == PAM_PROMPT_ECHO_OFF ||
            message.msg_style == PAM_PROMPT_ECHO_ON) {
            if (talk->answered) {
                talk->said += std::string("asks again: ") + message.msg + " ";
                for (int j = 0; j < i; ++j) {
                    free(answers[j].resp);
                }
                free(answers);
                return PAM_CONV_ERR;
            }
            answers[i].resp = strdup(talk->password->c_str()); // PAM frees it
            talk->answered = true;
        } else if (message.msg_style == PAM_TEXT_INFO ||
                   message.msg_style == PAM_ERROR_MSG) {
            talk->said += std::string(message.msg) + " ";
        }
    }
    *replies = answers;
    return PAM_SUCCESS;
}

Outcome check_pam(const std::string& user, const std::string& password,
                  const std::string& service)
{
    Talk conversation{&password, false, ""};
    const pam_conv callbacks = {talk, &conversation};
    pam_handle_t* handle = nullptr;
    int rc = pam_start(service.c_str(), user.c_str(), &callbacks, &handle);
    if (rc != PAM_SUCCESS) {
        return {Verdict::Failed,
                std::string("pam_start: ") + pam_strerror(handle, rc)};
    }
    rc = pam_authenticate(handle, PAM_DISALLOW_NULL_AUTHTOK);
    std::string text = pam_strerror(handle, rc);
    pam_end(handle, rc);
    if (!conversation.said.empty()) {
        text += " (" + conversation.said + ")";
    }
    if (rc == PAM_SUCCESS) {
        return {Verdict::Right, ""};
    }
    return {rc == PAM_AUTH_ERR || rc == PAM_MAXTRIES ? Verdict::Wrong
                                                     : Verdict::Failed,
            text};
}

#endif // TINY_HAVE_PAM

std::string find_helper(const std::string& wanted)
{
    if (!wanted.empty()) {
        return wanted;
    }
    for (const char* path :
         {TINY_UNIX_CHKPWD, "/usr/sbin/unix_chkpwd", "/sbin/unix_chkpwd",
          "/usr/bin/unix_chkpwd", "/usr/lib/unix_chkpwd"}) {
        if (path[0] != '\0' && access(path, X_OK) == 0) {
            return path;
        }
    }
    return "";
}

// Runs the helper with the user and the password on its standard input (the
// way pam_unix does it: the password and its terminating zero). The helper
// gets no environment and no output.
Outcome check_helper(const std::string& user, const std::string& password,
                     const std::string& helper)
{
    if (helper.empty()) {
        return {Verdict::Failed, "no unix_chkpwd found, use -H"};
    }
    int input[2];
    if (pipe(input) != 0) {
        return {Verdict::Failed, std::string("pipe: ") + strerror(errno)};
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(input[0]);
        close(input[1]);
        return {Verdict::Failed, std::string("fork: ") + strerror(errno)};
    }
    if (pid == 0) {
        dup2(input[0], STDIN_FILENO);
        const int null = open("/dev/null", O_WRONLY);
        dup2(null, STDOUT_FILENO);
        dup2(null, STDERR_FILENO);
        // execve() takes char*, but does not change the strings
        char* const arguments[] = {const_cast<char*>(helper.c_str()),
                                   const_cast<char*>(user.c_str()),
                                   const_cast<char*>("nonull"), nullptr};
        char* const no_environment[] = {nullptr};
        execve(helper.c_str(), arguments, no_environment);
        _exit(127);
    }
    close(input[0]);
    const char* bytes = password.c_str();
    size_t left = password.size() + 1;
    while (left > 0) {
        const ssize_t written = write(input[1], bytes, left);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            break;
        }
        bytes += written;
        left -= static_cast<size_t>(written);
    }
    close(input[1]);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return {Verdict::Failed,
                    std::string("waitpid: ") + strerror(errno)};
        }
    }
    if (!WIFEXITED(status)) {
        return {Verdict::Failed, "the helper was killed"};
    }
    switch (WEXITSTATUS(status)) {
        case 0:
            return {Verdict::Right, ""};
        case helper_wrong_password:
            return {Verdict::Wrong, ""};
        case 127:
            return {Verdict::Failed, "can't run " + helper};
        default:
            return {Verdict::Failed,
                    "the helper returned " +
                            std::to_string(WEXITSTATUS(status))};
    }
}

Outcome check(const Backend& backend, const std::string& user,
              const std::string& password)
{
#ifdef TINY_HAVE_PAM
    if (backend.use_pam) {
        return check_pam(user, password, backend.service);
    }
#endif
    return check_helper(user, password, backend.helper);
}

// A check in a child process: the answer comes through a pipe as one byte of
// Verdict and a text.
class Job {
  public:
    Job() = default;
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;

    ~Job()
    {
        if (fd >= 0) {
            close(fd);
        }
    }

    bool running() const { return fd >= 0; }

    int descriptor() const { return fd; }

    bool start(const Backend& backend, const std::string& user,
               const std::string& password)
    {
        int pipes[2];
        if (pipe(pipes) != 0) {
            return false;
        }
        child = fork();
        if (child < 0) {
            close(pipes[0]);
            close(pipes[1]);
            return false;
        }
        if (child == 0) {
            close(pipes[0]);
            const Outcome outcome = check(backend, user, password);
            const char verdict = static_cast<char>(outcome.verdict);
            if (write(pipes[1], &verdict, 1) == 1 && !outcome.text.empty()) {
                if (write(pipes[1], outcome.text.data(), outcome.text.size()) <
                    0) {
                    _exit(1);
                }
            }
            _exit(0); // not exit(): the buffers of Xlib are not ours to flush
        }
        close(pipes[1]);
        fd = pipes[0];
        return true;
    }

    // The answer of the child that has finished (or has died)
    Outcome finish()
    {
        std::string answer;
        char buffer[256];
        ssize_t got;
        while ((got = read(fd, buffer, sizeof buffer)) != 0) {
            if (got < 0 && errno == EINTR) {
                continue;
            }
            if (got < 0) {
                break;
            }
            answer.append(buffer, static_cast<size_t>(got));
        }
        close(fd);
        fd = -1;
        int status = 0;
        waitpid(child, &status, 0);
        if (answer.empty()) {
            return {Verdict::Failed, "the check ended without an answer"};
        }
        return {static_cast<Verdict>(answer[0]), answer.substr(1)};
    }

  private:
    int fd = -1;
    pid_t child = -1;
};

// Overwrite the text; volatile, so that the compiler keeps the writes
void wipe(std::string& text)
{
    volatile char* bytes = text.data();
    for (size_t i = 0; i < text.size(); ++i) {
        bytes[i] = 0;
    }
    text.clear();
}

// The password from the terminal without the echo (or a line from a pipe)
std::string read_password()
{
    termios saved;
    const bool terminal = tcgetattr(STDIN_FILENO, &saved) == 0;
    if (terminal) {
        termios quiet = saved;
        const tcflag_t echo = ECHO; // int on Linux, unsigned on NetBSD
        quiet.c_lflag &= ~echo;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet);
        fputs("Password: ", stderr);
    }
    // read(), not stdio: the password is not left in a buffer of the library
    std::string password;
    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != '\n') {
        password += c;
    }
    if (terminal) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
        fputs("\n", stderr);
    }
    return password;
}

// -c: no window, one check
int check_once(const Backend& backend, const std::string& user)
{
    std::string password = read_password();
    const Outcome outcome = check(backend, user, password);
    wipe(password);
    switch (outcome.verdict) {
        case Verdict::Right:
            puts("Correct");
            return 0;
        case Verdict::Wrong:
            printf("Wrong password%s%s\n", outcome.text.empty() ? "" : ": ",
                   outcome.text.c_str());
            return 1;
        default:
            printf("Can't check: %s\n", outcome.text.c_str());
            return 2;
    }
}

constexpr int spacing = 10;
constexpr int line_height = 20;
constexpr int entry_height = 28;
constexpr int window_width = 440;

class PasswordWindow : public tiny::Box {
  public:
    PasswordWindow(Backend backend_arg, std::string user_arg):
        tiny::Box(tiny::Box::Type::Vertical, window_width, window_height()),
        backend(std::move(backend_arg)),
        user(std::move(user_arg)),
        title(inner_width(), line_height, "Password of " + user),
        entry(inner_width() - 2 * tiny::theme.entry.border_width, entry_height),
        result(inner_width(), line_height, "")
    {
        realize(XDefaultRootWindow(display), 100, 100);
        push_start(&title, spacing, spacing);
        push_start(&entry, spacing, spacing / 2);
        push_start(&result, spacing, spacing / 2);

        entry.set_password();
        entry.on_activate.connect(
                [this](tiny::Object*, const XEvent&, void*) { ask(); });
        entry.on_cancel.connect([this](tiny::Object*, const XEvent&, void*) {
            running = false;
        });
        entry.on_change.connect([this](tiny::Object*, const XEvent&, void*) {
            if (!job.running()) {
                result.set_text("");
            }
        });

        // NOLINTNEXTLINE(clang-analyzer-optin.cplusplus.VirtualCall): nothing derives from PasswordWindow
        set_events();
        XStoreName(display, window, "Password");
        fprintf(stderr, "Checking with %s\n",
                backend.use_pam ? ("PAM, service " + backend.service).c_str()
                                : ("the helper " + backend.helper).c_str());
        map_all();
    }

    void set_events(long mask = 0) override
    {
        tiny::Box::set_events(mask);

        Atom wm_delete_window = display.WM_DELETE_WINDOW;
        XSetWMProtocols(display, window, &wm_delete_window, 1);
        connect(ClientMessage,
                [this](const XEvent& e) { on_client_message(e); });
    }

    void run()
    {
        while (running) {
            while (XPending(display) > 0) {
                XEvent event;
                XNextEvent(display, &event);
                const auto signal_id =
                        std::make_pair(event.type, event.xany.window);
                if (tiny::handlers.count(signal_id)) {
                    tiny::handlers.call_handler(signal_id, event);
                }
            }
            if (!running) {
                break;
            }
            focus_when_shown();
            // A request that waits for a reply (the one above) reads the
            // events that came meanwhile into the queue of Xlib; poll() sees
            // only the socket, so look at the queue before it waits.
            if (XPending(display) > 0) {
                continue;
            }
            pollfd fds[2] = {{XConnectionNumber(display), POLLIN, 0},
                             {job.descriptor(), POLLIN, 0}};
            if (poll(fds, job.running() ? 2 : 1, -1) < 0 && errno != EINTR) {
                break;
            }
            if (job.running() && fds[1].revents != 0) {
                show(job.finish());
            }
        }
    }

  private:
    static int inner_width() { return window_width - 2 * spacing; }

    static int window_height()
    {
        return spacing + line_height + spacing / 2 + entry_height +
               2 * tiny::theme.entry.border_width + spacing / 2 + line_height +
               spacing;
    }

    // With a window manager the window is mapped later, when the manager
    // decides (XSetInputFocus on a window that is not viewable is BadMatch),
    // so the focus is given on the first pass of the loop after that.
    void focus_when_shown()
    {
        if (focused) {
            return;
        }
        XWindowAttributes attributes;
        if (XGetWindowAttributes(display, entry.get_window(), &attributes) !=
                    0 &&
            attributes.map_state == IsViewable) {
            entry.set_focus();
            focused = true;
        }
    }

    void ask()
    {
        if (job.running() || entry.get_text().empty()) {
            return;
        }
        if (!job.start(backend, user, entry.get_text())) {
            result.set_text("Can't start the check");
            return;
        }
        // the child has its copy; ours is overwritten at once
        entry.clear();
        result.set_text("Checking...");
    }

    void show(const Outcome& outcome)
    {
        std::string text;
        switch (outcome.verdict) {
            case Verdict::Right:
                text = "Correct";
                break;
            case Verdict::Wrong:
                text = "Wrong password";
                break;
            default:
                text = "Can't check";
                break;
        }
        if (!outcome.text.empty()) {
            text += ": " + outcome.text;
        }
        result.set_text(text);
        printf("%s\n", text.c_str());
        fflush(stdout);
    }

    void on_client_message(const XEvent& e)
    {
        if (e.xclient.message_type == display.WM_PROTOCOLS &&
            static_cast<Atom>(e.xclient.data.l[0]) ==
                    display.WM_DELETE_WINDOW) {
            running = false;
        }
    }

    Backend backend;
    std::string user;
    tiny::Label title;
    tiny::Entry entry;
    tiny::Label result;
    Job job;
    bool running = true;
    bool focused = false; //!< the entry has the keyboard focus
};

void usage(const char* program)
{
    fprintf(stderr,
            "Usage: %s [-c] [-b pam|helper] [-s PAM-service] [-H helper]\n"
            "  -c  ask on the terminal, check once, no window\n"
            "  -b  how the password is checked (default: %s)\n"
            "  -s  the PAM service, default tiny-example\n"
            "  -H  the helper that checks the password of the user from its\n"
            "      standard input, default: unix_chkpwd\n",
            program,
#ifdef TINY_HAVE_PAM
            "pam"
#else
            "helper; this program was built without PAM"
#endif
    );
}

} // namespace

int main(int argc, char* argv[])
{
    Backend backend;
#ifdef TINY_HAVE_PAM
    backend.use_pam = true;
#endif
    bool terminal_only = false;
    int option;
    while ((option = getopt(argc, argv, "cb:s:H:h")) != -1) {
        switch (option) {
            case 'c':
                terminal_only = true;
                break;
            case 'b':
                if (strcmp(optarg, "pam") == 0) {
#ifdef TINY_HAVE_PAM
                    backend.use_pam = true;
#else
                    fprintf(stderr, "Built without PAM (see make PAM=...)\n");
                    return 2;
#endif
                } else if (strcmp(optarg, "helper") == 0) {
                    backend.use_pam = false;
                } else {
                    usage(argv[0]);
                    return 2;
                }
                break;
            case 's':
                backend.service = optarg;
                break;
            case 'H':
                backend.helper = optarg;
                break;
            default:
                usage(argv[0]);
                return option == 'h' ? 0 : 2;
        }
    }
    if (!backend.use_pam) {
        backend.helper = find_helper(backend.helper);
    }
    const std::string user = user_name();
    if (user.empty()) {
        fprintf(stderr, "Unknown user\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    if (terminal_only) {
        return check_once(backend, user);
    }
    tiny::Display::init();
    tiny::theme.init(); // opens the fonts
    {
        PasswordWindow window(backend, user);
        window.run();
    }
    tiny::theme.release(); // before the display is closed
    return 0;
}
