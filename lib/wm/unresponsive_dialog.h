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

    //! Show the dialog for the application `app`; `others` more windows of
    //! its process go away with it
    void popup(const std::string& app, unsigned others = 0);

    //! Center the dialog over the window `frame` of the application and stack
    //! it right above, like a dialog of that window
    void place_over(::Window frame);

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
