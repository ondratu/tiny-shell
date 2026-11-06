#pragma once

#include <functional>
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

    //! The programs that Tab completes, read at the first Tab after the dialog
    //! is shown (a program that was installed since is there); `$PATH` when
    //! not set
    std::function<std::vector<std::string>()> list_programs;

  private:
    void complete();

    tiny::Rect screen = {0, 0, 0, 0};
    tiny::Label title;
    tiny::Entry entry;
    CommandCompleter completer;
    bool programs_known = false; //!< the completer has the programs
};

} // namespace tiny::wm
