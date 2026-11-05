#pragma once

#include <string>

#include "button.h"
#include "container.h"
#include "label.h"
#include "wm/system_info.h"

namespace tiny::wm {

//! The about dialog: the name of the shell, its version and the system; a
//! click outside of it or on Close hides it
class AboutDialog : public tiny::Popover {
  public:
    AboutDialog();

    ~AboutDialog() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    //! Show the dialog in the middle of the screen
    void popup(const std::string& version, const SystemInfo& info);

    //! The texts of the lines, from the top
    const std::string& line(int index) const;

    static constexpr int line_count = 5;

  private:
    tiny::Label title;
    tiny::Label lines[line_count - 1];
    tiny::LabelButton close;
};

} // namespace tiny::wm
