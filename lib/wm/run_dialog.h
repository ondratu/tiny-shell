#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "container.h"
#include "entry.h"
#include "label.h"
#include "wm/command_completer.h"

namespace tiny::wm {

//! Alt+F2: a title and a field for a command line that is started by Return;
//! Tab completes the name of a program from `PATH`
class RunDialog : public tiny::Popover {
  public:
    RunDialog();

    ~RunDialog() override;

    void realize(::Window root, int x = 0, int y = 0) override;
    void popup();

    //! The monitor where the dialog opens, in its middle; without it the root
    //! window
    void set_screen(const tiny::Rect& monitor) { screen = monitor; }
    void popover(Object* o, const XEvent& e, void* data);

    void run(Object* o, const XEvent& e, void* data);

    //! The command line typed so far
    const std::string& get_text() const { return entry.get_text(); }

    //! The programs that Tab completes, read in a thread of their own each time
    //! the dialog is shown (a program that was installed since is there), so a
    //! slow disk does not stop the event loop; `$PATH` when not set. It runs
    //! outside of the main thread and must not use X.
    std::function<std::vector<std::string>()> list_programs;

    //! The programs are read: Tab completes. Before that Tab does nothing,
    //! the user presses it again.
    bool programs_ready() const;

  private:
    void complete();
    void read_programs();

    tiny::Rect screen = {0, 0, 0, 0};
    tiny::Label title;
    tiny::Entry entry;
    CommandCompleter completer;
    //! The reading of the programs in a thread of its own (pthread_create, not
    //! `std::async`: that is 11 kB of the binary); the thread is detached and
    //! keeps its own reference, so a dialog that goes away does not wait for it
    struct Reading;
    static void* read_in_thread(void* arg);
    std::shared_ptr<Reading> reading;
    bool programs_taken = false; //!< the completer has what the reading gave
};

} // namespace tiny::wm
