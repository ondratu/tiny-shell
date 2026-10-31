#pragma once

#include "style.h"

namespace tiny::wm {

class ButtonStyle : public tiny::Style {
  public:
    ButtonStyle()
    {
        // Normal state
        normal_bg = 0xF8F8F8;
        normal_fg = 0x2E3436;

        disable_fg = 0x8B8E8F;

        hover_bg = 0x2E3436;
        hover_fg = 0xF8F8F8;

        border_width = 0;
    }
}; // ButtonStyle

class HeaderStyle : public tiny::Style {
  public:
    HeaderStyle()
    {
        normal_bg = 0xF8F8F8;
        normal_fg = 0x0E1416;

        disable_fg = 0x6E7476;

        border_width = 0;
        padding_width = 2;

        font_style = ":bold";
    }

}; // HeaderStyle

class DockStyle : public tiny::Style {
  public:
    DockStyle()
    {
        border_width = 0;
        padding_width = 5;
    }

}; // DockStyle

} // namespace tiny::wm
