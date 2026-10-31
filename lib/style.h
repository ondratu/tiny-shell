#pragma once

#include <stdint.h>
#include <string>

#include <X11/Xft/Xft.h>

namespace tiny {

/**
 * @short Round a metric multiplied by the scale factor.
 */
int scaled(int value, double factor);

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

    Style() = default;
    virtual ~Style();

    void init();

    // Multiply border, padding and font size of the base values by factor
    void scale(double factor);

    uint32_t get_bg(uint8_t state) const;
    uint32_t get_fg(uint8_t state) const;
    uint32_t get_br(uint8_t state) const;
    XftFont* get_font() const { return font; }

    std::string get_xft_fg(uint8_t state) const;

    // Normal state
    uint32_t normal_bg = BLACK;
    uint32_t normal_fg = 0xA8A8A8;
    uint32_t normal_br = 0x2E3436;

    // Disable
    uint32_t disable_bg = INHERITED;
    uint32_t disable_fg = 0x555753;
    uint32_t disable_br = INHERITED;

    // Active (on mouse hover)
    uint32_t hover_bg = 0x2E3436;
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
    uint32_t selected_bg = 0x3E4446;
    uint32_t selected_fg = INHERITED;
    uint32_t selected_br = INHERITED;

    int border_width = 1;
    int padding_width = 1;

  protected:
    const char* font_family = "Cantarell";
    const char* font_style = ""; // e.g. ":bold"
    double font_size = 12;
    XftFont* font = nullptr;

  private:
    bool base_set = false;
    int base_border_width = 0;
    int base_padding_width = 0;
    double base_font_size = 0;

}; // Style

// Text entry: a dark gray field that gets a light border while it has focus
class EntryStyle : public Style {
  public:
    EntryStyle()
    {
        normal_bg = 0x2E3436;
        normal_fg = 0xF3F3F3;
        normal_br = 0x555753;

        disable_fg = 0x8B8E8F;

        focus_br = 0x729FCF;

        padding_width = 6;
    }
}; // EntryStyle

} // namespace tiny
