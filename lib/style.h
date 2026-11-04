#pragma once

#include <stdint.h>
#include <string>

#include <X11/Xft/Xft.h>

namespace tiny {

/**
 * @short Round a metric multiplied by the scale factor.
 */
int scaled(int value, double factor);

/**
 * @short The name of a font for Xft: `family-size:style`.
 *
 * The characters that the fontconfig syntax gives a meaning (`\`, `-`, `:`
 * and `,`) are escaped in the family, so any name of an installed font opens
 * that font. `style` is added as it is (e.g. ":bold").
 */
std::string xft_name(const std::string& family, double size,
                     const std::string& style = "");

/**
 * @short The part of an Xft name that asks for a style by its name
 * (`:style=Bold Italic`), escaped like the family; empty for an empty style.
 */
std::string xft_style(const std::string& style);

//! The colors of a theme: the shell has two, `Dark` is the default
enum class Variant : uint8_t { Dark, Light };

//! "dark" or "light" (any case) to the variant; false for any other name
bool variant_from_name(const std::string& name, Variant& variant);

const char* variant_name(Variant variant);

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

    Style() { Style::colorize(Variant::Dark); }
    virtual ~Style();

    void init();

    //! Set all the colors for the variant; the metrics and the font stay, so
    //! a style can change its look while widgets use it
    virtual void colorize(Variant variant);

    // Multiply border, padding and font size of the base values by factor
    void scale(double factor);

    uint32_t get_bg(uint8_t state) const;
    uint32_t get_fg(uint8_t state) const;
    uint32_t get_br(uint8_t state) const;
    XftFont* get_font() const { return font; }

    //! The family of the font; takes effect at the next `init()`
    void set_font_family(const std::string& family) { font_family = family; }

    //! The size at the scale 1, in points; takes effect at the next `scale()`
    //! and `init()`
    void set_font_size(double size)
    {
        base_font_size = size;
        font_size = size;
    }

    //! The style of the font as the part of an Xft name, see `xft_style()`
    void set_font_style(const std::string& style) { font_style = style; }

    std::string get_xft_fg(uint8_t state) const;

    // Normal state
    uint32_t normal_bg = 0;
    uint32_t normal_fg = 0;
    uint32_t normal_br = 0;

    // Disable
    uint32_t disable_bg = INHERITED;
    uint32_t disable_fg = 0;
    uint32_t disable_br = INHERITED;

    // Active (on mouse hover)
    uint32_t hover_bg = 0;
    uint32_t hover_fg = 0;
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
    uint32_t selected_bg = 0;
    uint32_t selected_fg = INHERITED;
    uint32_t selected_br = INHERITED;

    int border_width = 1;
    int padding_width = 1;

  protected:
    std::string font_family = "Cantarell";
    std::string font_style; // e.g. ":bold"
    double font_size = 12;
    XftFont* font = nullptr;

  private:
    bool base_set = false;
    int base_border_width = 0;
    int base_padding_width = 0;
    double base_font_size = 0;

}; // Style

// Text entry: a field that gets a light border while it has focus
class EntryStyle : public Style {
  public:
    EntryStyle()
    {
        padding_width = 6;
        EntryStyle::colorize(Variant::Dark);
    }

    void colorize(Variant variant) override;
}; // EntryStyle

// List of rows: no border, the selected row differs from the others
class ListStyle : public Style {
  public:
    ListStyle()
    {
        border_width = 0;
        padding_width = 6;
        ListStyle::colorize(Variant::Dark);
    }

    void colorize(Variant variant) override;
}; // ListStyle

} // namespace tiny
