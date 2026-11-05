#pragma once

#include "widget.h"

namespace tiny::wm {

/**
 * @short An invisible strip along the bottom edge of the screen that shows a
 * hidden dock when the pointer reaches it.
 *
 * It is an input only window: it draws nothing and takes pointer events only
 * in the few pixels of its height, which the owner takes from the theme.
 */
class DockReveal : public tiny::Widget {
  public:
    explicit DockReveal(int height);
    ~DockReveal() override;

    void set_events(long mask = 0) override;

    /// The pointer came into the strip
    tiny::Signal on_reach;

  private:
    void on_enter_notify(const XEvent& e);
};

} // namespace tiny::wm
