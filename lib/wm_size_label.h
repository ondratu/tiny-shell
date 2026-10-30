#pragma once

#include <string>

#include "widget.h"

namespace wm {

/**
 * @short Text on a small dark box in the center of an area, e.g. the size of
 * a terminal while it is resized. The box is only as big as the text, so
 * the content around it stays visible.
 */
class SizeLabel : public tiny::Widget {
  public:
    SizeLabel(int width, int height);

    ~SizeLabel() override;

    void set_events(long mask = 0) override;

    //! The area (e.g. of a client) in which the label stays in the center
    void set_area(int width, int height, int y);

    void set_text(const std::string& text);

    const std::string& get_text() const { return text; }

    int get_border() const override { return 0; }

  protected:
    void on_expose(const XEvent& e, void* data);

  private:
    //! Returns true when the size of the box changed
    bool center();

    std::string text;
    int area_width = 0;
    int area_height = 0;
    int area_y = 0;
};

} // namespace wm
