#pragma once

#include <string>

#include "button.h"
#include "container.h"
#include "label.h"

namespace tiny::wm {

//! Asks what to do with an application that does not respond: wait or quit
class UnresponsiveDialog : public tiny::Container {
  public:
    UnresponsiveDialog();

    ~UnresponsiveDialog() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    //! Show the dialog for the application `name`; `others` more windows of
    //! its process go away with it
    void popup(const std::string& name, unsigned others = 0);

    //! The user wants to wait, or to quit the application
    tiny::Signal on_wait;
    tiny::Signal on_quit;

  private:
    tiny::Label title;
    tiny::Label detail;
    tiny::LabelButton wait;
    tiny::LabelButton quit;
};

} // namespace tiny::wm
