#pragma once

#include <stdint.h>
#include <string_view>

#include <X11/Xft/Xft.h>

namespace tiny {

class Style {
    /* State heredity
     * Invisible - widget is not visible (out of allowed drawing area)
     * Normal - base state
     * Selected -> overwrite Normal
     * Hover -> overwrite only Normal, Selected
     * Focus -> overwrite Normal, Selected, Hover
     * Pressed -> overwrite Normal, Selected, Hover, Focus, Pressed
     * Disabled -> can't be overwritten
     * */

  public:
    enum class State : uint8_t {
        Invisible = 0,
        Normal = 1,
        Selected = 2,
        Hover = 4,
        Focus = 8,
        Pressed = 16,
        Disabled = 32
    };

    // last byte is 0x01
    static constexpr uint32_t INHERITED = 0x01FFFFFF;

    // last byte is 0x00
    static constexpr uint32_t BLACK = 0x000000;
    static constexpr uint32_t WHITE = 0xFFFFFF;
    static constexpr uint32_t GRAY25 = 0x404040;
    static constexpr uint32_t GRAY50 = 0x808080;
    static constexpr uint32_t GRAY75 = 0xc0c0c0;

  public:
    Style(){};
    virtual ~Style();

    void init();

    const uint32_t get_bg(uint8_t state) const;
    const uint32_t get_fg(uint8_t state) const;
    const uint32_t get_br(uint8_t state) const;
    XftFont* get_font() const
    { return font; }

    const std::string_view get_xft_fg(uint8_t state) const;

    // Normal state
    uint32_t normal_bg = BLACK;
    uint32_t normal_fg = 0xA8A8A8;
    uint32_t normal_br = 0x2E3436;

    // Disable
    uint32_t disable_bg = INHERITED;
    uint32_t disable_fg = 0x555753;
    uint32_t disable_br = INHERITED;

    // Active (on mouse hover)
    uint32_t hover_bg = INHERITED;
    uint32_t hover_fg = 0xF3F3F3;
    uint32_t hover_br = INHERITED;

    // Focus (on keyboard cursor)
    uint32_t focus_bg = INHERITED;
    uint32_t focus_fg = INHERITED;
    uint32_t focus_br = INHERITED;

    // Pressed or clicked
    uint32_t pressed_bg = INHERITED;
    uint32_t pressed_fg = INHERITED;
    uint32_t pressed_br = INHERITED;

    // Selected
    uint32_t selected_bg = INHERITED;
    uint32_t selected_fg = INHERITED;
    uint32_t selected_br = INHERITED;

    uint32_t border_width = 1;
    uint32_t padding_width = 1;

  private:
    std::string_view font_name = "Cantarell-12";
    XftFont* font;

}; // Style


class WMButtonStyle: public Style {
  public:
      WMButtonStyle(){
        // Normal state
        //normal_bg = 0x2E3436;
        normal_bg = BLACK;
        normal_fg = 0xF8F8F8;
        normal_br = 0xB6B6B3;

        disable_fg = 0x8B8E8F;

        hover_bg = GRAY25;
        hover_fg = 0xF7F7F7;
        hover_br = 0xF8F8F7;
      }
}; // WMButtonStyle


class WMHeaderStyle: public Style {
  public:
      WMHeaderStyle(){
        normal_bg = 0xF8F8F8;
        normal_fg = 0x0E1416;
        normal_br = 0x888A85;

        disable_fg = 0x6E7476;
      }

    uint32_t padding = 2;
  private:
    std::string_view font_name = "Cantarell-12:bold";

}; // WMHeaderStyle

class Theme {
    public:
        Theme(){}
        ~Theme(){}

        void init();

        uint32_t root_background = 0x729FCF;
        uint32_t wm_win_header = 25;
        uint32_t wm_win_border = 10;
        uint32_t wm_win_corner = wm_win_border*2;
        uint32_t wm_win_min_width = 10;
        uint32_t wm_win_min_height = 10+wm_win_header;

        uint32_t wm_panel = 30;
        uint32_t wm_icon = 48;
        uint32_t wm_dock_border = 5;

        Style widget;
        WMButtonStyle wm_button;
        WMHeaderStyle wm_header;
}; // Theme

extern Theme theme;

} // namespace

// default tiny-shel widgets are dark
