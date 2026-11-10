#pragma once

#include <X11/Xft/Xft.h>

#include <string>

#include "widget.h"

namespace tiny {

/**
 * @short Static text without a border, aligned to the left or to the middle.
 *
 * The label does not size itself (the owner makes the window), but it says how
 * big its text is: `natural_height()` is one line of the font and
 * `text_width()` the width of the text, so a layout can be computed from them.
 * `set_font_scale()` draws the text bigger (or smaller) than the font of the
 * style.
 */
class Label : public Widget {
  public:
    enum class Align { Left, Center };

    Label(int width, int height, const std::string& text = "");

    ~Label() override;

    void set_events(long mask = 0) override;

    void set_text(const std::string& text);

    const std::string& get_text() const { return text; }

    int get_border() const override { return 0; }

    //! Where the text is: at the left padding (the default) or in the middle
    void set_align(Align value);

    /**
     * @short The size of the font as a multiple of the font of the style.
     *
     * 1 is the font of the style. Call it before the label is drawn; the new
     * font is opened from the one of the style (the same family and style).
     */
    void set_font_scale(double factor);

    //! One line of the font: the ascent and the descent, and the padding
    int natural_height() const;

    //! The width of the text now (without the padding)
    int text_width() const;

  protected:
    void on_expose(const XEvent& e, void* data);

  private:
    XftFont* current_font() const;
    void close_font();

    std::string text;
    Align align = Align::Left;
    XftFont* own_font = nullptr;  //!< when the scale is not 1
    unsigned own_font_serial = 0; //!< the display that `own_font` belongs to
};

} // namespace tiny
