// default tiny-shell widgets are dark
#include "theme.h"

namespace tiny {

Theme theme; // default_theme

Theme::Theme()
{
    scale(1);
}

void Theme::scale(double factor)
{
    scale_factor = factor;

    widget.scale(factor);
    entry.scale(factor);
    list.scale(factor);
}

void Theme::set_font(const std::string& family)
{
    widget.set_font_family(family);
    entry.set_font_family(family);
    list.set_font_family(family);
}

void Theme::set_font_size(double size)
{
    widget.set_font_size(size);
    entry.set_font_size(size);
    list.set_font_size(size);
}

void Theme::set_font_style(const std::string& style)
{
    const std::string suffix = xft_style(style);
    widget.set_font_style(suffix);
    entry.set_font_style(suffix);
    list.set_font_style(suffix);
}

void Theme::set_variant(Variant new_variant)
{
    variant = new_variant;
    ++generation;
    widget.colorize(variant);
    entry.colorize(variant);
    list.colorize(variant);
}

void Theme::init() // TODO: screen, display, config...
{
    widget.init();
    entry.init();
    list.init();
}

void Theme::release()
{
    widget.release();
    entry.release();
    list.release();
}

} // namespace tiny
