#include "app/lock_pam.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <cstring>

#ifdef TINY_HAVE_PAM
#include <security/pam_appl.h>
#endif

namespace tiny::app {

std::vector<std::string> pam_directories()
{
    return {"/etc/pam.d", "/usr/lib/pam.d", "/usr/etc/pam.d"};
}

namespace {

//! The text of the file of the service, "" if there is none
std::string read_service(const std::string& service,
                         const std::vector<std::string>& directories)
{
    for (const std::string& directory : directories) {
        std::string file = directory;
        file += '/';
        file += service;
        std::ifstream in(file);
        if (in) {
            return {std::istreambuf_iterator<char>(in),
                    std::istreambuf_iterator<char>()};
        }
    }
    return "";
}

} // namespace

bool pam_service_denies(const std::string& service,
                        const std::vector<std::string>& directories)
{
    std::string text = read_service(service, directories);
    if (text.empty()) {
        text = read_service("other", directories); // the fallback of PAM
    }
    std::istringstream lines(text);
    std::string line;
    bool any = false;
    while (std::getline(lines, line)) {
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line[start] == '#') {
            continue;
        }
        const bool is_auth =
                line.compare(start, 4, "auth") == 0 &&
                line.size() > start + 4 &&
                std::isspace(static_cast<unsigned char>(line[start + 4])) != 0;
        const bool include = line[start] == '@' ||
                             line.find("include", start) != std::string::npos ||
                             line.find("substack", start) != std::string::npos;
        if (include && (is_auth || line[start] == '@')) {
            return false; // it leads somewhere else
        }
        if (is_auth) {
            if (line.find("pam_deny", start) == std::string::npos) {
                return false;
            }
            any = true;
        }
    }
    return any;
}

#ifdef TINY_HAVE_PAM

namespace {

struct Talk {
    Dialog* dialog;
    bool cancelled = false;
};

void free_answers(pam_response* answers, int count)
{
    for (int i = 0; i < count; ++i) {
        if (answers[i].resp != nullptr) {
            wipe(answers[i].resp, strlen(answers[i].resp));
            free(answers[i].resp);
        }
    }
    free(answers);
}

// The conversation function: PAM frees the answers it gets
int talk(int count, const pam_message** messages, pam_response** replies,
         void* data)
{
    auto* talk = static_cast<Talk*>(data);
    if (count <= 0) {
        return PAM_CONV_ERR;
    }
    auto* answers = static_cast<pam_response*>(
            calloc(static_cast<size_t>(count), sizeof(pam_response)));
    if (answers == nullptr) {
        return PAM_BUF_ERR;
    }
    for (int i = 0; i < count; ++i) {
        const pam_message& message = *messages[i];
        const std::string text = message.msg != nullptr ? message.msg : "";
        switch (message.msg_style) {
            case PAM_PROMPT_ECHO_OFF:
            case PAM_PROMPT_ECHO_ON: {
                std::string answer;
                if (!talk->dialog->ask(message.msg_style == PAM_PROMPT_ECHO_OFF,
                                       text, answer)) {
                    talk->cancelled = true;
                    free_answers(answers, count);
                    return PAM_CONV_ERR;
                }
                answers[i].resp = strdup(answer.c_str());
                wipe(answer);
                if (answers[i].resp == nullptr) {
                    free_answers(answers, count);
                    return PAM_BUF_ERR;
                }
                break;
            }
            case PAM_ERROR_MSG:
                talk->dialog->say(true, text);
                break;
            default: // PAM_TEXT_INFO
                talk->dialog->say(false, text);
                break;
        }
    }
    *replies = answers;
    return PAM_SUCCESS;
}

} // namespace

bool pam_built()
{
    return true;
}

Outcome check_pam(const std::string& user, const std::string& service,
                  Dialog& dialog)
{
    Talk conversation{&dialog, false};
    const pam_conv callbacks = {talk, &conversation};
    pam_handle_t* handle = nullptr;
    int rc = pam_start(service.c_str(), user.c_str(), &callbacks, &handle);
    if (rc != PAM_SUCCESS) {
        return {Verdict::Failed,
                std::string("pam_start: ") + pam_strerror(handle, rc)};
    }
    rc = pam_authenticate(handle, PAM_DISALLOW_NULL_AUTHTOK);
    const std::string text = pam_strerror(handle, rc);
    pam_end(handle, rc);
    if (rc == PAM_SUCCESS) {
        return {Verdict::Right, ""};
    }
    if (conversation.cancelled) {
        return {Verdict::Failed, "cancelled"};
    }
    if (rc == PAM_AUTH_ERR || rc == PAM_MAXTRIES) {
        return {Verdict::Wrong, text};
    }
    return {Verdict::Failed, text};
}

#else // no PAM

bool pam_built()
{
    return false;
}

Outcome check_pam(const std::string&, const std::string&, Dialog&)
{
    return {Verdict::Failed, "this program was built without PAM"};
}

#endif

} // namespace tiny::app
