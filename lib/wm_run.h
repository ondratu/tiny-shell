#pragma once

#include "containers.h"
#include "buttons.h"

namespace wm {

class Entry : public tiny::Widget {
  public:
    Entry(int width, int height);

    ~Entry() override;

    void realize(::Window parent, int x, int y) override;
    void set_events(long mask = 0) override;
    void set_focus();
    void clear();

    tiny::Signal on_exit;

  protected:
    void on_key_release(const XEvent& e, void* data);
    void on_expose(const XEvent& e, void* data);
    void on_focus_in(const XEvent& e, void* data);
    void on_focus_out(const XEvent& e, void* data);

  private:
    std::string text;

    XIM xim;
    XIC xic;
};

class RunDialog : public tiny::Popover {
  public:
    RunDialog();

    ~RunDialog() override;

    void realize(::Window root, int x = 0, int y = 0) override;
    void popup();
    void popover(Object* o, const XEvent& e, void* data);

  private:
    Entry entry;
};

} // namespace wm
