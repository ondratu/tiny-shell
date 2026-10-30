#pragma once

#include <string>

#include "widget.h"

namespace tiny {

//! Static text, aligned to the left, without a border.
class Label : public Widget {
  public:
    Label(int width, int height, const std::string& text = "");

    ~Label() override;

    void set_events(long mask = 0) override;

    void set_text(const std::string& text);

    const std::string& get_text() const { return text; }

    int get_border() const override { return 0; }

  protected:
    void on_expose(const XEvent& e, void* data);

  private:
    std::string text;
};

} // namespace tiny
