#include "plugin/widgets.h"

#include <algorithm>

#include "wm/shell_theme.h"

namespace tiny::plugin {

namespace {

int extent(::Display* display, XftFont* font, const std::string& text)
{
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &extents);
    return extents.xOff;
}

} // namespace

int panel_text_width(::Display* display, int padding, XftFont* font,
                     const std::string& text, const std::string& sample)
{
    return std::max(extent(display, font, text),
                    extent(display, font, sample)) +
           2 * padding;
}

PluginLabel::PluginLabel(const std::string& initial, const std::string& sample):
    tiny::Label(1, wm::shell_theme.panel, initial)
{
    name = "plugin_label";
    resize(panel_text_width(display, get_padding(), get_style().get_font(),
                            initial, sample),
           wm::shell_theme.panel);
}

PluginLabel::~PluginLabel() = default;

PluginButton::PluginButton(const std::string& initial,
                           const std::string& sample):
    tiny::LabelButton(1, wm::shell_theme.panel, initial)
{
    name = "plugin_button";
    set_border(0);
    resize(panel_text_width(display, get_padding(), get_style().get_font(),
                            initial, sample),
           wm::shell_theme.panel);
}

PluginButton::~PluginButton() = default;

} // namespace tiny::plugin
