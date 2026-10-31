#pragma once

#include "container.h"
#include "entry.h"
#include "label.h"

namespace tiny::wm {

//! Alt+F2: a title and a field for a command line that is started by Return
class RunDialog : public tiny::Popover {
  public:
    RunDialog();

    ~RunDialog() override;

    void realize(::Window root, int x = 0, int y = 0) override;
    void popup();
    void popover(Object* o, const XEvent& e, void* data);

    void run(Object* o, const XEvent& e, void* data);

  private:
    tiny::Label title;
    tiny::Entry entry;
};

} // namespace tiny::wm
