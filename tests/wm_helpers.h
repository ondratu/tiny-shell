#pragma once

// The helpers of the tests of the shell; the ones of the widgets are in
// helpers.h (the tests of the widgets must not need the shell)
#include "helpers.h"
#include "wm/shell_theme.h"

// The screen below the panel, the work area without any dock.
inline tiny::Rect screen_area()
{
    XWindowAttributes a;
    XGetWindowAttributes(tiny::get_display(), root_window(), &a);
    return {0, tiny::wm::shell_theme.panel, a.width,
            a.height - tiny::wm::shell_theme.panel};
}

// Same as WidgetFont for wm::shell_theme.header.
class HeaderFont {
  public:
    HeaderFont(): original(tiny::wm::shell_theme.header), with_font(original)
    {
        with_font.init();
        tiny::wm::shell_theme.header = with_font;
    }

    ~HeaderFont() { tiny::wm::shell_theme.header = original; }

  private:
    tiny::wm::HeaderStyle original;
    tiny::wm::HeaderStyle with_font;
};
