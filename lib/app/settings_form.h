#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "text_field.h"
#include "widget.h"
#include "wm/config.h"

namespace tiny::app {

/**
 * @short What the settings program shows and does, without any drawing.
 *
 * The form has a panel at its left with the sections (Appearance, Fonts, Desktop, Displays, Power, Applications) and the fields of the chosen section: text fields (the font,
 * its size and style, the scale, the format of the clock, the wallpaper, the
 * terminal), choices (antialiasing, hinting and subpixel order of the text,
 * the placement of the wallpaper, the mode of the dock: the left and right
 * arrows, Space and a click go through the values), a button that opens the
 * list of fonts, and the buttons Cancel and Save. The fields are edited with a
 * cursor: the arrows, Home, End, Backspace and Delete work, and a click puts
 * the cursor where the pointer is. Tab goes through the fields of the section,
 * Ctrl+Page Down and Ctrl+Page Up through the sections.
 *
 * The list of fonts replaces the form: a search field above the families, one
 * family selected, and under the list the size and the style of the font and
 * a preview of a text in the selected family, size and style. Select (or
 * Return) takes the three into the form, Cancel (or Escape) goes back and
 * leaves them as they were.
 *
 * The owner (`tiny-settings`) tells the size of the window and the input,
 * asks for the rectangles to draw, how wide a text is (`set_measure`, for the
 * pointer) and writes the file when `result()` says Saved. All the numbers
 * are in pixels, except the size of the font, which is in points.
 *
 * A click is a press and a release on the same thing, so a press that moves
 * away before the release does nothing.
 */
class SettingsForm {
  public:
    enum class View { Form, Picker };

    //! The groups of the settings, in the panel at the left
    enum class Section {
        Appearance,
        Fonts,
        Desktop,
        Displays,
        Power,
        Security,
        Applications
    };

    enum class Field {
        Font,
        Size,
        Style,
        Terminal,
        Scale,
        Antialias, //!< a choice, see `xft_option()`
        Hinting,
        Rgba,
        Wallpaper, //!< the path of a PNG or JPEG, typed (there is no browser)
        WallpaperMode,    //!< a choice: fill, center or tile
        ClockFormat,      //!< a `strftime()` format; empty is no clock
        Dock,             //!< a choice: auto-hide, on or off
        PrimaryMonitor,   //!< auto (empty), a number or the name of an output
        SecondaryMonitor, //!< the same
        MonitorPlacement, //!< a choice: keep, right, left, above or below
        LockDelay,  //!< a choice: the minutes from the display off to the lock
        DisplayOff, //!< a choice: the minutes to switch the display off
        Theme,      //!< a choice: the style of the theme, dark or light
        PanelStyle, //!< a choice: the panel follows the theme, light or dark
        DockStyle,  //!< a choice: the same for the dock
        LockStyle,  //!< a choice: the same for the lock screen
        LauncherStyle //!< a choice: the same for the launcher
    };

    //! What has the keyboard in the list of fonts
    enum class Pane { Search, Size, Style };

    enum class Key {
        Up,
        Down,
        Left,
        Right,
        PageUp,
        PageDown,
        Home,
        End,
        Return,
        Back, //!< backspace
        Delete,
        Escape,
        Tab,
        BackTab,     //!< Shift+Tab
        NextSection, //!< Ctrl+Page Down
        PrevSection  //!< Ctrl+Page Up
    };

    //! Editing goes on until the user saves (Saved) or gives up (Cancelled)
    enum class Result { Editing, Saved, Cancelled };

    //! The texts of the fields, as the user sees them
    struct Values {
        std::string font;
        std::string terminal;
        std::string scale;
        std::string size = "12"; //!< in points at the scale 1
        std::string style;       //!< "" is the default style of the family
        //! The values of `[xft]` ("" is not set)
        std::string antialias;
        std::string hinting;
        std::string rgba;
        //! The path of the wallpaper ("" is none) and how it covers the screen
        // NOLINTBEGIN(readability-redundant-member-init) ; the initializer
        // keeps -Wmissing-field-initializers quiet where a form is made
        std::string wallpaper{};
        std::string wallpaper_mode{};
        //! The format of the clock of the panel ("" is no clock) and the mode
        //! of the dock ("" is auto-hide, then "on" and "off")
        std::string clock_format{};
        std::string dock{};
        //! The monitors of `[monitors]`: "" is auto and keep
        std::string primary_monitor{};
        std::string secondary_monitor{};
        std::string monitor_placement{};
        //! The minutes from the display off to the lock ("" is never, then
        //! 0, 1, 2, 5, 10 and 30)
        std::string lock_delay{};
        //! The minutes without input to switch the display off ("" leaves
        //! the X server as it is, then "never", 5, 10, 15, 30 and 60)
        std::string display_off{};
        //! The style of the theme ("" is dark, then "light") and the looks of
        //! the panel and the dock ("" follows the theme, then "light" and
        //! "dark")
        std::string theme{};
        std::string panel_style{};
        std::string dock_style{};
        std::string lock_style{};     //!< like the panel: "", light, dark
        std::string launcher_style{}; //!< the same
        // NOLINTEND(readability-redundant-member-init)
    };

    //! The sizes of the parts, at the scale 1
    struct Metrics {
        int margin = 16;
        int sidebar_width = 150; //!< the panel with the sections
        int padding = 8;         //!< inside of a field, left and right
        int label_width = 90;    //!< the labels left of the fields
        int row_height = 34;     //!< a field, a button, a row of the list
        int row_gap = 14;        //!< between the fields
        int note_height = 52;    //!< the three small lines under a field
        int button_width = 100;  //!< Save and Cancel
        int choose_width = 120;  //!< the button that opens the list of fonts
        int text_size = 15;      //!< the font of the text, in pixels
        int scroll_step = 3;     //!< the rows of one notch of the wheel
        int preview_rows = 3;    //!< the height of the preview, in rows

        //! Every size multiplied by the factor and rounded
        static Metrics scaled(double factor);
    };

    //! The names of the styles of a family
    using StylesFn =
            std::function<std::vector<std::string>(const std::string&)>;

    SettingsForm(const Values& initial, std::vector<std::string> families,
                 const Metrics& metrics, StylesFn styles_of = nullptr);

    /// How wide a text is, so that a click can find a place in it.
    void set_measure(TextField::Measure measure)
    {
        measure_ = std::move(measure);
    }

    /// The monitors that the fields Primary and Secondary offer, besides
    /// Auto: "1: HDMI-1 1920x1080". The value of a choice is the name of the
    /// output (the number when it has none). Call it before the form is used.
    void set_monitors(const wm::Monitors& monitors);

    /// The value that the form uses for the monitor a setting names ("auto",
    /// a number or an output): "" is auto, else the name or the number.
    static std::string monitor_value(const wm::Monitors& monitors,
                                     const std::string& spec);

    /// The size of the window.
    void resize(int width, int height);

    /// The height that shows the form without scrolling.
    int form_height() const;

    View view() const { return view_; }

    Result result() const { return result_; }

    Field focus() const { return focus_; }

    // The sections

    static constexpr size_t section_count = 7;

    /// The section that shows.
    Section section() const { return section_; }

    /// Show a section; the focus goes to its first field.
    void set_section(Section section);

    /// The name of a section, as the panel shows it.
    static const char* section_title(Section section);

    /// The fields of a section, from the top.
    static const std::vector<Field>& section_fields(Section section);

    /// The section a field is in.
    static Section section_of(Field field);

    /// The panel with the sections, and the item of one of them.
    Rect sidebar_rect() const;
    Rect section_rect(Section section) const;

    /// A short help for a field; empty when it has none.
    static const char* hint(Field field);

    /// What a field does, in small lines under it (three at most); empty when it has no
    /// note. A section where a field has one has taller rows.
    static const char* note(Field field);
    static bool section_has_notes(Section section);

    /// The texts of the fields.
    Values values() const;

    /// The field, with the text and the cursor.
    const TextField& field(Field field) const;

    /// Whether the field is a choice of a few values (it has no cursor).
    static bool is_choice(Field field);

    /// The option of `[xft]` that a choice sets; the field must be Antialias,
    /// Hinting or Rgba.
    static wm::XftOption xft_option(Field field);

    /// The name of the value of a choice, as the user sees it.
    std::string choice_label(Field field) const;

    /// The text of a field.
    const std::string& text(Field field) const
    {
        return this->field(field).text();
    }

    /// What is wrong in a field, empty when the text can be saved.
    std::string problem(Field field) const;

    /// What is wrong in the first field that has a problem; empty when none.
    std::string problem() const;

    /// The values to write: the texts of the fields without the blanks at both
    /// ends.
    Values clean_values() const;

    /// The message under the fields (a failed write); cleared by an edit.
    const std::string& status() const { return status_; }

    /// The write failed: editing goes on and `message` is shown.
    void reject(const std::string& message);

    /// How far to the left the text of a field is moved so that its cursor
    /// shows, in a field with `room` pixels for the text (zero or less).
    int shift(const TextField& field, int room) const;

    // The form

    Rect label_rect(Field field) const;
    Rect field_rect(Field field) const;
    //! The place of the note under a field (see `note()`)
    Rect note_rect(Field field) const;
    Rect choose_rect() const;
    //! Cancel; in the list of fonts it goes back
    Rect cancel_rect() const;
    //! Save; in the list of fonts it is Select
    Rect save_rect() const;

    /// Where the small picture of the wallpaper is drawn: between the first and
    /// the second field of the Appearance (above the path of the file).
    Rect thumbnail_rect() const;

    /// Where the problem, and under it the hint, are written.
    Rect message_rect() const;

    // The list of fonts

    Pane pane() const { return pane_; }

    const TextField& search_field() const { return search_; }

    const std::string& query() const { return search_.text(); }

    /// The families that match the search.
    const std::vector<std::string>& families() const { return shown_; }

    /// The index in `families()` of the selected one.
    size_t selected() const { return selected_; }

    /// The first row that shows.
    size_t first() const { return first_; }

    /// How many rows fit under the search field (at least one).
    size_t visible_rows() const;

    Rect search_rect() const;

    /// The row of the list in the coordinates of the window, for `first()` + n.
    Rect row_rect(size_t index) const;

    const TextField& size_field() const { return pick_size_; }

    /// The style that is chosen, "" is the default one.
    const std::string& pick_style() const { return pick_style_; }

    /// The styles of the selected family.
    const std::vector<std::string>& pick_styles() const { return pick_styles_; }

    /// What is wrong with the size in the list, empty when it is right.
    std::string pick_problem() const;

    Rect size_label_rect() const;
    Rect size_rect() const;
    Rect style_label_rect() const;
    Rect style_rect() const;

    /// Where the preview is drawn.
    Rect preview_rect() const;

    /// The selected family, empty when there is none.
    const std::string& preview_family() const;

    /// The size of the preview in points (at the scale 1); the size of the
    /// form when the one in the list is wrong.
    double preview_size() const;

    /// The text of the preview.
    static const char* preview_text();

    /// A button went down at the point.
    void press(int x, int y);

    /// The button went up; acts when it is on the thing that was pressed.
    /// True when something changed.
    bool release(int x, int y);

    /// The wheel by notches, positive goes down; true when something changed.
    bool wheel(int notches);

    /// A key; true when something changed.
    bool key(Key pressed);

    /// A text that was typed (in UTF-8): it goes to the focused field or to
    /// the search; true when something changed.
    bool type(const std::string& typed);

    /// Show the list of fonts at the font, the size and the style of the form.
    void open_picker();

    /// Take the selected family, size and style into the form and show it.
    /// False (and the list stays) when the size is wrong.
    bool choose();

    /// Show the form again, the font is as it was.
    void close_picker();

  private:
    enum Target : size_t {
        NoTarget,
        FontField,
        SizeField,
        StyleField,
        TerminalField,
        ScaleField,
        AntialiasField,
        HintingField,
        RgbaField,
        WallpaperField,
        WallpaperModeField,
        ClockFormatField,
        DockField,
        PrimaryMonitorField,
        SecondaryMonitorField,
        MonitorPlacementField,
        LockDelayField,
        DisplayOffField,
        ThemeField,
        PanelStyleField,
        DockStyleField,
        LockStyleField,
        LauncherStyleField, //!< the fields are in the order of `Field`
        ChooseButton,
        CancelButton,
        SaveButton,
        SearchBox,
        SizeBox,
        StyleBox,
        SectionAppearance, //!< the items of the panel, in the order of `Section`
        SectionFonts,
        SectionDesktop,
        SectionDisplays,
        SectionPower,
        SectionSecurity,
        SectionApplications,
        FirstRow //!< and the rows of the list from here
    };

    //! The values of a choice
    static wm::XftChoices choices_of(Field field);

    //! The values of a choice and the names a form shows for them; the
    //! monitors are the ones of `set_monitors()`
    std::vector<std::pair<std::string, std::string>>
    options_of(Field field) const;
    TextField& field_ref(Field field);
    size_t target_at(int x, int y) const;
    bool act(size_t target, int x);
    bool request_save();
    bool edit(TextField& field, Key pressed);
    void step_choice(Field field, int direction);
    bool form_key(Key pressed);
    bool picker_key(Key pressed);
    bool list_key(Key pressed);
    bool size_step(int direction);
    bool cycle_style(int direction);
    bool select(size_t index);
    void search(const std::string& query);
    void refresh_styles();
    void put_cursor(TextField& field, const Rect& rect, int x);
    void reveal();
    size_t max_first() const;
    int rows_top() const;
    int row_pitch(Section section) const; //!< a row, the gap and the note
    int controls_top() const;

    std::array<TextField, 22> fields_;
    //! value and label of the monitors, Auto first
    std::vector<std::pair<std::string, std::string>> monitor_options_ = {
            {"", "Auto"}};
    TextField search_;
    TextField pick_size_;
    std::string pick_style_;
    std::vector<std::string> pick_styles_;
    std::map<std::string, std::vector<std::string>> styles_cache_;
    StylesFn styles_of_;
    TextField::Measure measure_;
    std::vector<std::string> all_;
    std::vector<std::string> shown_;
    std::string status_;
    Metrics metrics_;
    View view_ = View::Form;
    Result result_ = Result::Editing;
    Field focus_ = Field::Theme;
    Section section_ = Section::Appearance;
    Pane pane_ = Pane::Search;
    int width_ = 1;
    int height_ = 1;
    size_t selected_ = 0;
    size_t first_ = 0;
    size_t pressed_ = NoTarget;
};

} // namespace tiny::app
