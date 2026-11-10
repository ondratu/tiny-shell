#include "app/lock_pam.h"

#include <cstdlib>
#include <cstring>

#ifdef TINY_HAVE_PAM
#include <security/pam_appl.h>
#endif

namespace tiny::app {

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
