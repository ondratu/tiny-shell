#pragma once
#include <string>

#include "container.h"
#include "theme.h"

namespace tiny::wm {

class TitleBox : public tiny::Widget {
  public:
    TitleBox(int width, int height);

    ~TitleBox() override;

    void set_events(long mask = 0) override;

    tiny::Signal on_drag_begin;
    tiny::Signal on_drag_end;
    tiny::Signal on_drag_motion;

  protected:
    void on_button_press(const XEvent& e, void* data);
    void on_button_release(const XEvent& e, void* data);
    void on_motion_notify(const XEvent& e, void* data);
};

class Header : public tiny::Box {
  public:
    Header(int width, int height);

    ~Header() override;

    void set_events(long mask = 0) override;

    void realize(::Window parent, int x, int y) override;

    void resize(int width, int height) override;

    std::string get_title() const { return title; }

    void set_title(const std::string& title);

    TitleBox* get_title_box() { return &title_box; }

    void set_disable(bool disable);

    const tiny::Style& get_style() const override
    {
        return tiny::theme.wm_header;
    }

  protected:
    void on_expose(const XEvent& e, void* data);

  private:
    std::string title;

    TitleBox title_box;
    const int screen; // XXX: this is zero for now
};

} // namespace tiny::wm
