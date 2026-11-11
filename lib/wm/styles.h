#pragma once

#include "style.h"

namespace tiny::wm {

// The buttons and the title bar of the windows follow the variant: they are
// light in the light one and dark in the dark one
class ButtonStyle : public tiny::Style {
  public:
    ButtonStyle()
    {
        border_width = 0;
        ButtonStyle::colorize(Variant::Dark);
    }

    void colorize(Variant variant) override
    {
        Style::colorize(variant);
        const bool dark = variant == Variant::Dark;
        normal_bg = dark ? 0x303436 : 0xF8F8F8;
        normal_fg = dark ? 0xEEEEEC : 0x2E3436;

        disable_fg = dark ? 0x6E7476 : 0x8B8E8F;

        hover_bg = dark ? 0x555753 : 0x2E3436;
        hover_fg = dark ? 0xFFFFFF : 0xF8F8F8;
    }
}; // ButtonStyle

class HeaderStyle : public tiny::Style {
  public:
    HeaderStyle()
    {
        border_width = 0;
        padding_width = 2;

        font_style = ":bold";
        HeaderStyle::colorize(Variant::Dark);
    }

    void colorize(Variant variant) override
    {
        Style::colorize(variant);
        const bool dark = variant == Variant::Dark;
        normal_bg = dark ? 0x303436 : 0xF8F8F8;
        normal_fg = dark ? 0xEEEEEC : 0x0E1416;
        normal_br = dark ? 0x1A1D1F : 0xD8D6D4;

        disable_fg = dark ? 0x888A85 : 0x6E7476;
    }

}; // HeaderStyle

class DockStyle : public tiny::Style {
  public:
    DockStyle()
    {
        border_width = 0;
        padding_width = 5;
    }

    //! The bar under the icon of a window that demands attention
    uint32_t attention = 0xE95420;

}; // DockStyle

//! A flat colored area without a border (a line, a mark)
class PlainStyle : public tiny::Style {
  public:
    PlainStyle(uint32_t dark_color, uint32_t light_color):
        dark(dark_color), light(light_color)
    {
        border_width = 0;
        padding_width = 0;
        PlainStyle::colorize(Variant::Dark);
    }

    void colorize(Variant variant) override
    {
        Style::colorize(variant);
        normal_bg = variant == Variant::Dark ? dark : light;
    }

  private:
    uint32_t dark;
    uint32_t light;
}; // PlainStyle

} // namespace tiny::wm
