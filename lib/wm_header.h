#pragma once
#include <string>

#include "containers.h"
#include "theme.h"

namespace wm {

class TitleBox: public tiny::Widget {
  public:
    TitleBox(uint32_t width, uint32_t height);

    virtual ~TitleBox();

    virtual void set_events(long mask=0);

    tiny::Signal on_drag_begin;
    tiny::Signal on_drag_end;
    tiny::Signal on_drag_motion;

  protected:
    void on_button_press(const XEvent &e, void *data);
    void on_button_release(const XEvent &e, void *data);
    void on_motion_notify(const XEvent &e, void *data);
};


class Header: public tiny::Box {
  public:
    Header(uint32_t width, uint32_t height);

    virtual ~Header();

    virtual void set_events(long mask=0);

    virtual void realize(::Window parent, int x, int y);

    virtual void resize(uint32_t width, uint32_t height);

    inline const std::string get_title() const
    { return title; }

    void set_title(const std::string &title);

    inline TitleBox* get_title_box()
    { return &title_box; }

    void set_disable(bool disable);

    inline virtual const tiny::Style& get_style() const
    { return tiny::theme.wm_header; }

  protected:
        void on_expose(const XEvent &e, void *data);

  private:
    std::string title;

    TitleBox title_box;
    const int screen;       // XXX: this is zero for now
};

} // namespace wm
