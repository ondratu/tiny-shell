#pragma once

#include "container.h"
#include "button.h"
#include "wm/window.h"

namespace tiny::wm {

class DockButton : public tiny::Widget {
  public:
    DockButton();
    ~DockButton() override;

    int get_border() const override { return 0; }

    void set_events(long mask = 0) override;

    void redraw(const XEvent& e) { on_expose(e, nullptr); }

    tiny::Signal on_click;

  protected:
    virtual void on_enter_notify(const XEvent& e, void* data);
    virtual void on_leave_notify(const XEvent& e, void* data);
    virtual void on_button_release(const XEvent& e, void* data);
    virtual void on_expose(const XEvent& e, void* data);
};

class DockIcon : public DockButton {
  public:
    DockIcon(Window* task);
    ~DockIcon() override;

  protected:
    void on_button_release(const XEvent& e, void* data) override;
    void on_expose(const XEvent& e, void* data) override;

    Window* task;
};

class Dock : public tiny::Box {
  public:
    Dock();
    ~Dock() override;

    using tiny::Box::add;
    using tiny::Box::remove;
    void add(Window* window);
    void remove(Window* window);
    void redraw(Window* window, const XEvent& e);

    void realize(::Window parent, int x, int y) override;
    void resize(int width, int height) override;

    const tiny::Style& get_style() const override
    {
        return tiny::theme.wm_dock;
    }

  private:
    void resize_to_children();

    DockButton icon;
    std::map<Window*, DockIcon*> wm_task_icons;
};

} // namespace tiny::wm
