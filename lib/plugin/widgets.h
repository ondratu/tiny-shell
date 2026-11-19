#pragma once

#include <string>

#include "button.h"
#include "label.h"

namespace tiny::plugin {

//! The width of the widest of the texts in the font of the widget, with the
//! padding, so a widget that changes its text does not move the others
int panel_text_width(::Display* display, int padding, XftFont* font,
                     const std::string& text, const std::string& sample);

/**
 * @short The text of a plugin in the panel.
 *
 * The width is fixed when the label is made: the wider of the text and of
 * `sample` (the longest text that the plugin will show). A longer text is cut
 * by the window.
 */
class PluginLabel : public tiny::Label {
  public:
    PluginLabel(const std::string& initial, const std::string& sample);

    ~PluginLabel() override;
};

//! The text of a plugin in the panel that a click on it is told to the plugin
class PluginButton : public tiny::LabelButton {
  public:
    PluginButton(const std::string& initial, const std::string& sample);

    ~PluginButton() override;
};

} // namespace tiny::plugin
