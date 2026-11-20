#pragma once

#include <functional>
#include <string>
#include <vector>

#include <memory>

#include "button.h"
#include "container.h"
#include "input_method.h"
#include "label.h"
#include "text_field.h"
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

//! `percent` of `towards` and the rest of `base`: the color of a row that the
//! pointer or the keyboard is on
uint32_t mixed(uint32_t base, uint32_t towards, int percent);

//! Clear the window and draw the icon and then the text, vertically centered;
//! `center` puts the text of a widget without an icon in the middle
void paint_item(::Display* display, ::Window window, const Style& style,
                uint8_t state, int width, int height, XftFont* font,
                const std::string& text, const ItemIcon& icon, int padding,
                bool center);

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
    PopupRow() = default;
    PopupRow(std::string icon_arg, std::string label_arg,
             std::string value_arg):
        icon(std::move(icon_arg)),
        label(std::move(label_arg)),
        value(std::move(value_arg))
    {}

    std::string icon;
    std::string label;
    std::string value;
    //! A small icon before the value, at the right (a lock for a network that
    //! has a password)
    std::string badge;
    //! A click or Return on the row tells the plugin this number (the plugin
    //! keeps the functions); -1 is a line that only shows something
    int action = -1;
    bool entry = false;  //!< a field to type in, in place of the value
    bool secret = false; //!< the field shows dots, as for a password
    bool dim = false;    //!< shown weaker, and nothing happens on it
    //! What the window knows the row by when the plugin shows it again (the
    //! selection and the text typed stay); the label when empty
    std::string id;

    //! Whether the pointer and the keys can come to the row
    bool target() const { return (action >= 0 || entry) && !dim; }

    const std::string& key() const { return id.empty() ? label : id; }
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

//! A row of a window: the number of its section and the number in it
struct RowRef {
    int section = -1;
    int row = -1;

    bool valid() const { return section >= 0 && row >= 0; }
    bool operator==(const RowRef& o) const
    {
        return section == o.section && row == o.row;
    }
    bool operator!=(const RowRef& o) const { return !(*this == o); }
};

//! Where things are in a window of a plugin (computed from the sizes of the
//! fonts, so a test can look at it without a display)
struct PopupLayout {
    struct Line {
        int y = 0; //!< the top of the line
        int height = 0;
    };
    int width = 0;
    int height = 0; //!< of the whole content, also of what is scrolled away
    int header_height = 0;
    int field_width = 0;        //!< of the field of a row that has one
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

//! The rows that the pointer and the keys can come to, from the top
std::vector<RowRef> popup_targets(const PopupContent& content);

//! The row at `y` (from the top of the whole content, not of what is shown)
//! when the pointer can come to it; else an invalid one
RowRef popup_row_at(const PopupLayout& layout, const PopupContent& content,
                    int y);

//! `scroll` kept between the top and the last place that is worth showing
int clamp_scroll(int scroll, int content_height, int view_height);

//! The shift of the content that has the part from `top` to `bottom` in the
//! view, moved as little as it can be from `scroll`
int scroll_to_show(int scroll, int top, int bottom, int view_height);

/**
 * @short The window of a plugin that opens under a widget of the panel.
 *
 * A click outside of it closes it, as it does a menu. A window that is taller
 * than the screen has room for is scrolled by the wheel and the keys. The rows
 * that the plugin gave an action are lit under the pointer and by the arrow
 * keys, and Return or a click calls the action; a row with a field takes the
 * typed text, and Return gives it to the action.
 */
class PluginPopup : public tiny::Popover {
  public:
    //! What happens on a row: its action number, and the text of its field
    //! (empty for a row without one)
    using Action = std::function<void(int action, const std::string& text)>;

    explicit PluginPopup(wm::SymbolicIcons* icons);

    ~PluginPopup() override;

    void realize(::Window root, int x = 0, int y = 0) override;

    void set_events(long mask = 0) override;

    //! Also forgets the text that was typed
    void unmap() override;

    void set_action(Action function) { action = std::move(function); }

    //! The row that is lit
    RowRef selected_row() const { return selected; }

    //! How far the content is moved up
    int scrolled() const { return scroll; }

    //! The height of the part of the content that the window shows
    int view() const { return view_height; }

    //! Where the rows of the content are (the content is moved up by `scrolled()`)
    const PopupLayout& placed() const { return layout; }

    //! The text in the field, as it is typed
    const std::string& typed() const { return field.text(); }

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
    void on_key(const XEvent& e);
    void on_press(const XEvent& e);
    void on_motion(const XEvent& e);
    void select(RowRef row);
    void move_selection(int step);
    void scroll_by(int delta);
    void reveal_selected();
    void activate(RowRef row);
    const PopupRow* row_of(RowRef row) const;
    //! The text of the field as it is drawn, and where the cursor is in it
    std::string field_text(size_t* cursor) const;

    wm::SymbolicIcons* icons;
    PopupContent shown;
    PopupLayout layout;
    Action action;
    TextField field;
    std::string field_key; //!< the row that the text in the field was typed in
    std::unique_ptr<InputMethod> input;
    RowRef selected;
    int scroll = 0;
    int view_height = 0;
    int line_height = 0;
    XftFont* big_font = nullptr;
    unsigned big_font_serial = 0;
};

} // namespace tiny::plugin
