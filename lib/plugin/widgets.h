#pragma once

#include <functional>
#include <string>
#include <vector>

#include "button.h"
#include "container.h"
#include "label.h"
#include "wm/symbolic_icons.h"

namespace tiny::plugin {

//! The width of the widest of the texts in the font of the widget, with the
//! padding, so a widget that changes its text does not move the others
int panel_text_width(::Display* display, int padding, XftFont* font,
                     const std::string& text, const std::string& sample);

/**
 * @short The symbolic icon in front of the text of a widget of a plugin.
 *
 * The icon is drawn in the color of the text. The `under` icon is drawn
 * first and dimmed: a level (the battery, the signal) is the whole symbol
 * dimmed with the part that is full over it, see doc/developers/plugins.md.
 * The room for the icon is kept when the widget is made with an icon, so a
 * change of the icon does not move the other widgets.
 */
struct ItemIcon {
    std::string name;
    std::string under;
    bool room = false;                  //!< the width has the icon in it
    wm::SymbolicIcons* icons = nullptr; //!< null: no icons at all

    //! The size of the icon in a panel as high as `height`
    static int size_for(int height);

    //! The width that the icon takes with the gap after it; 0 without room
    int width_for(int height) const;
};

//! The colors of a text that is not the main one: halfway to the background
uint32_t dimmed(uint32_t foreground, uint32_t background);

//! Clear the window and draw the icon and then the text, vertically centered
void paint_item(::Display* display, ::Window window, const Style& style,
                uint8_t state, int height, XftFont* font,
                const std::string& text, const ItemIcon& icon, int padding);

/**
 * @short The text of a plugin in the panel, with an icon in front of it.
 *
 * The width is fixed when the label is made: the wider of the text and of
 * `sample` (the longest text that the plugin will show). A longer text is cut
 * by the window.
 */
class PluginLabel : public tiny::Label {
  public:
    PluginLabel(const std::string& initial, const std::string& sample,
                ItemIcon icon = {});

    ~PluginLabel() override;

    void set_icon(const std::string& icon, const std::string& under);

  protected:
    void on_expose(const XEvent& e, void* data) override;

  private:
    ItemIcon item;
};

//! The same that a click on it is told to the plugin
class PluginButton : public tiny::LabelButton {
  public:
    PluginButton(const std::string& initial, const std::string& sample,
                 ItemIcon icon = {});

    ~PluginButton() override;

    void set_icon(const std::string& icon, const std::string& under);

  protected:
    void on_expose(const XEvent& e, void* data) override;

  private:
    ItemIcon item;
};

//! One line of the window of a plugin: an icon, a name, a value at the right
struct PopupRow {
    std::string icon;
    std::string label;
    std::string value;
};

//! A group of the lines under a small heading
struct PopupSection {
    std::string heading;
    std::vector<PopupRow> rows;
};

/**
 * @short What a plugin shows in its window: a header and groups of lines.
 *
 * The header is a big icon, a title with a line under it and a big value
 * at the right (the weather: the symbol, the place with the description and
 * the temperature). Everything is optional.
 */
struct PopupContent {
    std::string icon;
    std::string title;
    std::string subtitle;
    std::string value;
    std::vector<PopupSection> sections;
};

//! Where things are in a window of a plugin (computed from the sizes of the
//! fonts, so a test can look at it without a display)
struct PopupLayout {
    struct Line {
        int y = 0; //!< the top of the line
        int height = 0;
    };
    int width = 0;
    int height = 0;
    int header_height = 0;
    std::vector<Line> headings; //!< per section
    std::vector<std::vector<Line>> rows;
};

/**
 * @short The numbers that the window of a plugin is laid out with.
 *
 * `line` is the height of a line of the text, `text_width()` the width of a
 * string; `limit` is the widest the window may be.
 */
PopupLayout
popup_layout(const PopupContent& content, int line, int icon_size, int big_line,
             int limit,
             const std::function<int(const std::string&)>& text_width,
             const std::function<int(const std::string&)>& big_width);

/**
 * @short The window of a plugin that opens under a widget of the panel.
 *
 * A click outside of it closes it, as it does a menu.
 */
class PluginPopup : public tiny::Popover {
  public:
    explicit PluginPopup(wm::SymbolicIcons* icons);

    ~PluginPopup() override;

    void set_events(long mask = 0) override;

    /**
     * @short Show `content` under `anchor`, as wide as it needs.
     *
     * `bounds` is the window of the panel: the popup stays inside of its
     * width. The popup must be realized.
     */
    void open(const PopupContent& content, ::Window anchor, ::Window bounds);

  protected:
    void on_expose(const XEvent& e, void* data);

  private:
    void close_font();

    wm::SymbolicIcons* icons;
    PopupContent shown;
    PopupLayout layout;
    XftFont* big_font = nullptr;
    unsigned big_font_serial = 0;
};

} // namespace tiny::plugin
