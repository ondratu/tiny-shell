#pragma once

#include <string>
#include <vector>

#include "button.h"
#include "container.h"
#include "label.h"
#include "wm/session_end.h"

namespace tiny::wm {

/**
 * @short The dialog of the logout, the restart and the shutdown.
 *
 * It asks whether to go on ("You will be logged out automatically in 60
 * seconds."), then it says which applications are still open while they close
 * ("Waiting for Firefox to close."), and it can report that the command that
 * restarts or shuts the computer down has failed. The shell owns the dialog and
 * its signals; the dialog does not decide anything.
 */
class SessionDialog : public tiny::Container {
  public:
    SessionDialog();

    ~SessionDialog() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    //! The monitor where the dialog opens, in its middle; without it the root
    //! window
    void set_screen(const tiny::Rect& monitor);

    //! "Log Out", the question with the seconds that are left, Cancel and the
    //! button that agrees
    void show_question(SessionKind kind, int seconds);

    //! The applications that are not closed yet; Cancel and "... Anyway"
    void show_waiting(SessionKind kind, const std::vector<std::string>& names);

    //! The command of a restart or a shutdown failed: the text and Close
    void show_failure(SessionKind kind, const std::string& command,
                      const std::string& reason);

    //! The user wants to go on (also "... Anyway"), or to cancel (also Close)
    tiny::Signal on_proceed;
    tiny::Signal on_cancel;

  private:
    void place();
    void set_buttons(const std::string& cancel_text,
                     const std::string& proceed_text);
    void set_lines(const std::string& new_title, const std::string& line1,
                   const std::string& line2);

    tiny::Rect screen = {0, 0, 0, 0};
    tiny::Label title;
    tiny::Label first;
    tiny::Label second;
    tiny::LabelButton cancel;
    tiny::LabelButton proceed;
};

} // namespace tiny::wm
