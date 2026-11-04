#pragma once

#include <stdint.h>

#include <string>

#include "style.h"

namespace tiny {

/**
 * @short The styles of the widgets of the toolkit.
 *
 * The toolkit knows nothing about window management; the metrics and the
 * styles of the shell are in `wm::ShellTheme`, which sets this one too.
 */
class Theme {
  public:
    // The scale is 1 until scale() is called (the configuration sets it)
    Theme();
    ~Theme() = default;

    void init();

    //! The family of the font of all the styles; call before `init()`
    void set_font(const std::string& family);

    //! The size in points at the scale 1, of all the styles; call before
    //! `scale()` and `init()`
    void set_font_size(double size);

    //! The style of the font (e.g. "Italic") of all the styles; empty is the
    //! default; call before `init()`
    void set_font_style(const std::string& style);

    // Scale all metrics and fonts (relative to the default theme)
    void scale(double factor);

    //! Change the colors of all the styles; widgets repaint with
    //! `Widget::restyle()`
    void set_variant(Variant variant);
    Variant get_variant() const { return variant; }

    //! Grows with every change of the variant: a widget that is not shown
    //! takes the new colors when it is mapped
    unsigned generation = 0;

    double scale_factor = 1;

    Style widget;
    EntryStyle entry;
    ListStyle list;

  private:
    Variant variant = Variant::Dark;
}; // Theme

extern Theme theme;

} // namespace tiny
