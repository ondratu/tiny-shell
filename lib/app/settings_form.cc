#include "app/settings_form.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <utility>

#include "ini.h"
#include "style.h"
#include "wm/config.h"
#include "xdg/desktop_entry.h"

namespace tiny::app {

namespace {

bool inside(const Rect& r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

//! The letters of ASCII in lower case, the other bytes as they are
char lower(char c)
{
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

//! Whether a name has the text in it, the case of ASCII letters is ignored
bool matches(const std::string& name, const std::string& query)
{
    return std::search(name.begin(), name.end(), query.begin(), query.end(),
                       [](char a, char b) { return lower(a) == lower(b); }) !=
           name.end();
}

bool equal_names(const std::string& a, const std::string& b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](char x, char y) { return lower(x) == lower(y); });
}

//! A typed text without the control characters (Return, Tab, Escape...)
std::string printable(const std::string& text)
{
    std::string rv;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (byte >= 0x20 && byte != 0x7f) {
            rv += c;
        }
    }
    return rv;
}

//! A size without the zeros that a number would have at the end ("12", "9.5")
std::string size_text(double size)
{
    char text[32];
    snprintf(text, sizeof text, "%g", size);
    return text;
}

//! The rows of the Appearance that the picture of the wallpaper takes
constexpr size_t thumbnail_rows = 3;

//! All the fields, in the order of `SettingsForm::Field` (the order of the
//! texts and of the targets)
constexpr SettingsForm::Field fields[] = {SettingsForm::Field::Font,
                                          SettingsForm::Field::Size,
                                          SettingsForm::Field::Style,
                                          SettingsForm::Field::Terminal,
                                          SettingsForm::Field::Scale,
                                          SettingsForm::Field::Antialias,
                                          SettingsForm::Field::Hinting,
                                          SettingsForm::Field::Rgba,
                                          SettingsForm::Field::Wallpaper,
                                          SettingsForm::Field::WallpaperMode,
                                          SettingsForm::Field::ClockFormat,
                                          SettingsForm::Field::Dock,
                                          SettingsForm::Field::PrimaryMonitor,
                                          SettingsForm::Field::SecondaryMonitor,
                                          SettingsForm::Field::MonitorPlacement,
                                          SettingsForm::Field::LockDelay,
                                          SettingsForm::Field::DisplayOff,
                                          SettingsForm::Field::DisplayDpms,
                                          SettingsForm::Field::Theme,
                                          SettingsForm::Field::PanelStyle,
                                          SettingsForm::Field::DockStyle,
                                          SettingsForm::Field::LockStyle,
                                          SettingsForm::Field::LauncherStyle,
                                          SettingsForm::Field::SessionAutostart,
                                          SettingsForm::Field::SessionConfirm,
                                          SettingsForm::Field::SessionPrograms};

} // namespace

SettingsForm::Metrics SettingsForm::Metrics::scaled(double factor)
{
    Metrics m;
    m.margin = tiny::scaled(m.margin, factor);
    m.sidebar_width = tiny::scaled(m.sidebar_width, factor);
    m.padding = tiny::scaled(m.padding, factor);
    m.label_width = tiny::scaled(m.label_width, factor);
    m.row_height = tiny::scaled(m.row_height, factor);
    m.row_gap = tiny::scaled(m.row_gap, factor);
    m.note_height = tiny::scaled(m.note_height, factor);
    m.button_width = tiny::scaled(m.button_width, factor);
    m.choose_width = tiny::scaled(m.choose_width, factor);
    m.text_size = tiny::scaled(m.text_size, factor);
    return m;
}

SettingsForm::SettingsForm(const Values& initial,
                           std::vector<std::string> families,
                           const Metrics& metrics, StylesFn styles_of):
    fields_{TextField(initial.font),
            TextField(initial.size),
            TextField(initial.style),
            TextField(initial.terminal),
            TextField(initial.scale),
            TextField(initial.antialias),
            TextField(initial.hinting),
            TextField(initial.rgba),
            TextField(initial.wallpaper),
            TextField(initial.wallpaper_mode),
            TextField(initial.clock_format),
            TextField(initial.dock),
            TextField(initial.primary_monitor),
            TextField(initial.secondary_monitor),
            TextField(initial.monitor_placement),
            TextField(initial.lock_delay),
            TextField(initial.display_off),
            TextField(initial.display_dpms),
            TextField(initial.theme),
            TextField(initial.panel_style),
            TextField(initial.dock_style),
            TextField(initial.lock_style),
            TextField(initial.launcher_style),
            TextField(initial.session_autostart),
            TextField(initial.session_confirm),
            TextField()},
    styles_of_(std::move(styles_of)),
    all_(std::move(families)),
    shown_(all_),
    metrics_(metrics)
{
    metrics_.row_height = std::max(1, metrics_.row_height);
    metrics_.scroll_step = std::max(1, metrics_.scroll_step);
}

void SettingsForm::resize(int width, int height)
{
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    first_ = std::min(first_, max_first());
    reveal();
}

int SettingsForm::form_height() const
{
    // the rows of the longest section, the message of two rows and the buttons
    int longest = 0;
    for (const Section section :
         {Section::Appearance, Section::Fonts, Section::Desktop,
          Section::Displays, Section::Power, Section::Security,
          Section::Applications, Section::Session}) {
        const auto rows = static_cast<int>(
                section_fields(section).size() +
                (section == Section::Appearance ? thumbnail_rows : 0));
        longest = std::max(longest, rows * row_pitch(section));
    }
    return 2 * metrics_.margin + longest + 3 * metrics_.row_height +
           metrics_.row_gap;
}

int SettingsForm::row_pitch(Section section) const
{
    return metrics_.row_height + metrics_.row_gap +
           (section_has_notes(section) ? metrics_.note_height : 0);
}

SettingsForm::Values SettingsForm::values() const
{
    return {text(Field::Font),
            text(Field::Terminal),
            text(Field::Scale),
            text(Field::Size),
            text(Field::Style),
            text(Field::Antialias),
            text(Field::Hinting),
            text(Field::Rgba),
            text(Field::Wallpaper),
            text(Field::WallpaperMode),
            text(Field::ClockFormat),
            text(Field::Dock),
            text(Field::PrimaryMonitor),
            text(Field::SecondaryMonitor),
            text(Field::MonitorPlacement),
            text(Field::LockDelay),
            text(Field::DisplayOff),
            text(Field::DisplayDpms),
            text(Field::Theme),
            text(Field::PanelStyle),
            text(Field::DockStyle),
            text(Field::LockStyle),
            text(Field::LauncherStyle),
            text(Field::SessionAutostart),
            text(Field::SessionConfirm)};
}

// The sections

const char* SettingsForm::section_title(Section section)
{
    switch (section) {
        case Section::Appearance:
            return "Appearance";
        case Section::Fonts:
            return "Fonts";
        case Section::Desktop:
            return "Desktop";
        case Section::Displays:
            return "Displays";
        case Section::Power:
            return "Power";
        case Section::Security:
            return "Security";
        case Section::Session:
            return "Session";
        case Section::Applications:
            break;
    }
    return "Applications";
}

const std::vector<SettingsForm::Field>&
SettingsForm::section_fields(Section section)
{
    static const std::vector<Field> fonts = {Field::Font,    Field::Size,
                                             Field::Style,   Field::Antialias,
                                             Field::Hinting, Field::Rgba};
    static const std::vector<Field> appearance = {
            Field::Theme,         Field::PanelStyle, Field::DockStyle,
            Field::LauncherStyle, Field::Scale,      Field::Wallpaper,
            Field::WallpaperMode};
    static const std::vector<Field> desktop = {Field::Dock, Field::ClockFormat};
    static const std::vector<Field> displays = {Field::PrimaryMonitor,
                                                Field::SecondaryMonitor,
                                                Field::MonitorPlacement};
    // TODO: the battery (what happens on low charge, power saver) goes here
    // when the shell can read it; Power is the place for it
    static const std::vector<Field> power = {Field::DisplayOff,
                                             Field::DisplayDpms};
    static const std::vector<Field> security = {Field::LockDelay,
                                                Field::LockStyle};
    static const std::vector<Field> applications = {Field::Terminal};
    static const std::vector<Field> session = {Field::SessionAutostart,
                                               Field::SessionPrograms,
                                               Field::SessionConfirm};
    switch (section) {
        case Section::Appearance:
            return appearance;
        case Section::Fonts:
            return fonts;
        case Section::Desktop:
            return desktop;
        case Section::Displays:
            return displays;
        case Section::Power:
            return power;
        case Section::Security:
            return security;
        case Section::Session:
            return session;
        case Section::Applications:
            break;
    }
    return applications;
}

SettingsForm::Section SettingsForm::section_of(Field field)
{
    for (const Section section :
         {Section::Appearance, Section::Fonts, Section::Desktop,
          Section::Displays, Section::Power, Section::Security,
          Section::Session}) {
        const std::vector<Field>& list = section_fields(section);
        if (std::find(list.begin(), list.end(), field) != list.end()) {
            return section;
        }
    }
    return Section::Applications;
}

void SettingsForm::set_section(Section section)
{
    if (section == section_) {
        return;
    }
    section_ = section;
    focus_ = section_fields(section).front();
    pressed_ = NoTarget;
}

const char* SettingsForm::hint(Field field)
{
    switch (field) {
        case Field::ClockFormat:
            return "A strftime format, e.g. %A %e %B %R; empty hides the "
                   "clock.";
        case Field::Dock:
            return "Auto-hide: hidden under a window, the bottom edge shows "
                   "it.";
        case Field::Theme:
            return "Dark or light, also for the programs.";
        case Field::PanelStyle:
        case Field::DockStyle:
        case Field::LockStyle:
        case Field::LauncherStyle:
            return "Or stay light or dark in any theme.";
        case Field::PrimaryMonitor:
            return "The monitor of the panel and the dock; the numbers are "
                   "on the screens.";
        case Field::SecondaryMonitor:
            return "The monitor that Position moves.";
        case Field::MonitorPlacement:
            return "Where the secondary monitor is, seen from the primary "
                   "one (runs xrandr).";
        case Field::Terminal:
            return "The program and its arguments, e.g. xterm -e.";
        case Field::SessionPrograms:
            return "Return or Space opens the list of the programs.";
        case Field::Wallpaper:
            return "The full path of a PNG or JPEG file.";
        default:
            break;
    }
    return "";
}

const char* SettingsForm::note(Field field)
{
    switch (field) {
        case Field::DisplayOff:
            return "Turns the display off after this time without input. "
                   "Unchanged keeps the setting of the X server.";
        case Field::DisplayDpms:
            return "The X server switches the display off as well. No leaves "
                   "DPMS alone, for drivers where it does nothing or hangs.";
        case Field::SessionAutostart:
            return "Starts the programs of ~/.config/autostart and "
                   "/etc/xdg/autostart when you log in.";
        case Field::SessionPrograms:
            return "Which programs of the autostart start: the ones of the "
                   "system and your own.";
        case Field::SessionConfirm:
            return "A dialog with a countdown before Logout, Restart and "
                   "Shutdown. Programs are asked to close either way.";
        case Field::LockDelay:
            return "Locks the screen this long after the display turns off "
                   "(see Power). Immediately locks it as the display goes off.";
        default:
            break;
    }
    return "";
}

bool SettingsForm::section_has_notes(Section section)
{
    const std::vector<Field>& list = section_fields(section);
    return std::any_of(list.begin(), list.end(),
                       [](Field f) { return *note(f) != '\0'; });
}

bool SettingsForm::is_choice(Field field)
{
    return field == Field::Antialias || field == Field::Hinting ||
           field == Field::Rgba || field == Field::WallpaperMode ||
           field == Field::Dock || field == Field::Theme ||
           field == Field::PanelStyle || field == Field::DockStyle ||
           field == Field::LockStyle || field == Field::LauncherStyle ||
           field == Field::PrimaryMonitor || field == Field::SecondaryMonitor ||
           field == Field::MonitorPlacement || field == Field::LockDelay ||
           field == Field::DisplayOff || field == Field::DisplayDpms ||
           field == Field::SessionAutostart || field == Field::SessionConfirm;
}

void SettingsForm::set_monitors(const wm::Monitors& monitors)
{
    monitor_options_ = {{"", "Auto"}};
    for (size_t i = 0; i < monitors.size(); ++i) {
        const wm::Monitor& m = monitors[i];
        monitor_options_.emplace_back(
                m.name.empty() ? std::to_string(i + 1) : m.name,
                std::to_string(i + 1) + ": " +
                        (m.name.empty() ? std::string("screen") : m.name) +
                        " " + std::to_string(m.area.width) + "x" +
                        std::to_string(m.area.height));
    }
}

std::string SettingsForm::monitor_value(const wm::Monitors& monitors,
                                        const std::string& spec)
{
    const auto found = wm::find_monitor(monitors, spec);
    if (!found) {
        return spec == "auto" ? "" : spec;
    }
    return monitors[*found].name.empty() ? std::to_string(*found + 1)
                                         : monitors[*found].name;
}

std::vector<std::pair<std::string, std::string>>
SettingsForm::options_of(Field field) const
{
    if (field == Field::PrimaryMonitor || field == Field::SecondaryMonitor) {
        return monitor_options_;
    }
    std::vector<std::pair<std::string, std::string>> options;
    for (const wm::XftChoice& c : choices_of(field)) {
        options.emplace_back(c.value, c.label);
    }
    return options;
}

wm::XftChoices SettingsForm::choices_of(Field field)
{
    if (field == Field::Dock) {
        return wm::dock_choices();
    }
    if (field == Field::Theme) {
        return wm::style_choices();
    }
    if (field == Field::PanelStyle || field == Field::DockStyle ||
        field == Field::LockStyle || field == Field::LauncherStyle) {
        return wm::look_choices();
    }
    if (field == Field::MonitorPlacement) {
        return wm::placement_choices();
    }
    if (field == Field::LockDelay) {
        return wm::lock_delay_choices();
    }
    if (field == Field::DisplayOff) {
        return wm::display_off_choices();
    }
    if (field == Field::DisplayDpms || field == Field::SessionAutostart ||
        field == Field::SessionConfirm) {
        return wm::dpms_choices(); // yes or no
    }
    return field == Field::WallpaperMode ? wm::wallpaper_choices()
                                         : wm::xft_choices(xft_option(field));
}

wm::XftOption SettingsForm::xft_option(Field field)
{
    switch (field) {
        case Field::Antialias:
            return wm::XftOption::Antialias;
        case Field::Hinting:
            return wm::XftOption::Hinting;
        default:
            break;
    }
    return wm::XftOption::Rgba;
}

std::string SettingsForm::choice_label(Field field) const
{
    for (const auto& option : options_of(field)) {
        if (text(field) == option.first) {
            return option.second;
        }
    }
    return text(field);
}

const TextField& SettingsForm::field(Field field) const
{
    return fields_[static_cast<size_t>(field)];
}

TextField& SettingsForm::field_ref(Field field)
{
    return fields_[static_cast<size_t>(field)];
}

std::string SettingsForm::problem(Field field) const
{
    const std::string& value = text(field);
    switch (field) {
        case Field::Font:
            return wm::valid_font(value)
                           ? ""
                           : "The font must be the name of a font family.";
        case Field::Size:
            return wm::parse_font_size(value)
                           ? ""
                           : "The size must be a number from 4 to 96.";
        case Field::Style:
            return wm::valid_font_style(value)
                           ? ""
                           : "The style must be a name without line breaks.";
        case Field::Terminal:
            return xdg::split_exec(value).empty()
                           ? "The terminal must be a program with arguments."
                           : "";
        case Field::Antialias:
        case Field::Hinting:
        case Field::Rgba:
            return wm::valid_xft(xft_option(field), value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::Wallpaper:
            return wm::valid_wallpaper_file(value)
                           ? ""
                           : "The wallpaper must be a path without line "
                             "breaks.";
        case Field::WallpaperMode:
            return wm::valid_wallpaper_mode(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::Dock:
            return wm::valid_dock_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::Theme:
            return wm::valid_style_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::PanelStyle:
        case Field::DockStyle:
        case Field::LockStyle:
        case Field::LauncherStyle:
            return wm::valid_look_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::LockDelay:
            return wm::valid_lock_delay_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::DisplayOff:
            return wm::valid_display_off_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::SessionPrograms: // not a text
            return "";
        case Field::DisplayDpms:
        case Field::SessionAutostart:
        case Field::SessionConfirm:
            return wm::valid_dpms_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::MonitorPlacement:
            return wm::valid_placement_choice(value)
                           ? ""
                           : "Choose one of the values that are offered.";
        case Field::PrimaryMonitor:
        case Field::SecondaryMonitor:
            return wm::valid_monitor_spec(value)
                           ? ""
                           : "A monitor is auto, a number or an output name "
                             "without blanks.";
        case Field::ClockFormat:
            return wm::valid_clock_format(value)
                           ? ""
                           : "The clock format must be a text without line "
                             "breaks.";
        case Field::Scale:
            break;
    }
    return wm::parse_scale(value)
                   ? ""
                   : "The scale must be a number above 0, up to 16.";
}

std::string SettingsForm::problem() const
{
    for (const Field f : fields) {
        std::string what = problem(f);
        if (!what.empty()) {
            return what;
        }
    }
    return "";
}

SettingsForm::Values SettingsForm::clean_values() const
{
    const auto clean = [](const std::string& value) {
        return std::string(trim(value));
    };
    const Values v = values();
    // the clock format keeps its blanks: they are part of the format
    return {clean(v.font),
            clean(v.terminal),
            clean(v.scale),
            clean(v.size),
            clean(v.style),
            clean(v.antialias),
            clean(v.hinting),
            clean(v.rgba),
            clean(v.wallpaper),
            clean(v.wallpaper_mode),
            v.clock_format,
            clean(v.dock),
            clean(v.primary_monitor),
            clean(v.secondary_monitor),
            clean(v.monitor_placement),
            clean(v.lock_delay),
            clean(v.display_off),
            clean(v.display_dpms),
            clean(v.theme),
            clean(v.panel_style),
            clean(v.dock_style),
            clean(v.lock_style),
            clean(v.launcher_style),
            clean(v.session_autostart),
            clean(v.session_confirm)};
}

void SettingsForm::reject(const std::string& message)
{
    result_ = Result::Editing;
    status_ = message;
}

int SettingsForm::shift(const TextField& field, int room) const
{
    if (!measure_) {
        return 0;
    }
    const int before = measure_(field.text().substr(0, field.cursor()));
    return std::min(0, room - before);
}

// The form: the rows of the fields, then the message and the buttons

//! The place of a field in its section, from the top (0 is the first)
static int row_of(SettingsForm::Field field)
{
    const std::vector<SettingsForm::Field>& list =
            SettingsForm::section_fields(SettingsForm::section_of(field));
    const auto row = static_cast<int>(
            std::find(list.begin(), list.end(), field) - list.begin());
    // the picture of the wallpaper is over its path
    const auto wallpaper =
            static_cast<int>(std::find(list.begin(), list.end(),
                                       SettingsForm::Field::Wallpaper) -
                             list.begin());
    return SettingsForm::section_of(field) ==
                                   SettingsForm::Section::Appearance &&
                           row >= wallpaper
                   ? row + static_cast<int>(thumbnail_rows)
                   : row;
}

Rect SettingsForm::sidebar_rect() const
{
    return {0, 0, metrics_.sidebar_width, height_};
}

Rect SettingsForm::section_rect(Section section) const
{
    return {0,
            metrics_.margin + static_cast<int>(section) * metrics_.row_height,
            metrics_.sidebar_width, metrics_.row_height};
}

Rect SettingsForm::label_rect(Field field) const
{
    const int y =
            metrics_.margin + row_of(field) * row_pitch(section_of(field));
    return {metrics_.sidebar_width + metrics_.margin, y, metrics_.label_width,
            metrics_.row_height};
}

Rect SettingsForm::field_rect(Field field) const
{
    const Rect label = label_rect(field);
    const int x = label.x + metrics_.label_width;
    // the font has the button that opens the list at its end
    const int reserved =
            field == Field::Font ? metrics_.choose_width + metrics_.row_gap : 0;
    return {x, label.y, std::max(1, width_ - x - metrics_.margin - reserved),
            metrics_.row_height};
}

Rect SettingsForm::note_rect(Field field) const
{
    const Rect r = field_rect(field);
    return {r.x, r.y + r.height + metrics_.row_gap / 4, r.width,
            metrics_.note_height - metrics_.row_gap / 4};
}

Rect SettingsForm::choose_rect() const
{
    const Rect label = label_rect(Field::Font);
    return {width_ - metrics_.margin - metrics_.choose_width, label.y,
            metrics_.choose_width, metrics_.row_height};
}

Rect SettingsForm::thumbnail_rect() const
{
    // the rows between the scale and the wallpaper
    const Rect first = field_rect(Field::Scale);
    const Rect after = field_rect(Field::Wallpaper);
    const int y = first.y + first.height + metrics_.row_gap;
    return {first.x, y, first.width,
            std::max(1, after.y - metrics_.row_gap - y)};
}

Rect SettingsForm::message_rect() const
{
    // over the buttons, the problem and under it the hint
    const int x = metrics_.sidebar_width + metrics_.margin;
    return {x, save_rect().y - metrics_.row_gap - 2 * metrics_.row_height,
            std::max(1, width_ - x - metrics_.margin), 2 * metrics_.row_height};
}

Rect SettingsForm::save_rect() const
{
    return {width_ - metrics_.margin - metrics_.button_width,
            height_ - metrics_.margin - metrics_.row_height,
            metrics_.button_width, metrics_.row_height};
}

Rect SettingsForm::cancel_rect() const
{
    Rect r = save_rect();
    r.x -= metrics_.row_gap + metrics_.button_width;
    return r;
}

// The list of fonts: the search field, the rows, the preview, the controls

int SettingsForm::rows_top() const
{
    return 2 * metrics_.margin + metrics_.row_height;
}

int SettingsForm::controls_top() const
{
    return save_rect().y - metrics_.row_gap - metrics_.row_height;
}

Rect SettingsForm::search_rect() const
{
    return {metrics_.margin, metrics_.margin,
            std::max(1, width_ - 2 * metrics_.margin), metrics_.row_height};
}

Rect SettingsForm::preview_rect() const
{
    const int height = metrics_.preview_rows * metrics_.row_height;
    return {metrics_.margin, controls_top() - metrics_.row_gap - height,
            std::max(1, width_ - 2 * metrics_.margin), height};
}

Rect SettingsForm::size_label_rect() const
{
    return {metrics_.margin, controls_top(), metrics_.label_width,
            metrics_.row_height};
}

Rect SettingsForm::size_rect() const
{
    return {metrics_.margin + metrics_.label_width, controls_top(),
            metrics_.button_width, metrics_.row_height};
}

Rect SettingsForm::style_label_rect() const
{
    const Rect size = size_rect();
    return {size.x + size.width + metrics_.row_gap, size.y,
            metrics_.label_width, metrics_.row_height};
}

Rect SettingsForm::style_rect() const
{
    const Rect label = style_label_rect();
    const int x = label.x + label.width;
    return {x, label.y, std::max(1, width_ - metrics_.margin - x),
            metrics_.row_height};
}

size_t SettingsForm::visible_rows() const
{
    const int bottom =
            (view_ == View::Programs ? message_rect().y : preview_rect().y) -
            metrics_.row_gap;
    const int rows = (bottom - rows_top()) / metrics_.row_height;
    return static_cast<size_t>(std::max(1, rows));
}

Rect SettingsForm::row_rect(size_t index) const
{
    const auto n = static_cast<int>(index) - static_cast<int>(first_);
    return {0, rows_top() + n * metrics_.row_height, width_,
            metrics_.row_height};
}

size_t SettingsForm::max_first() const
{
    const size_t rows = visible_rows();
    return list_size() > rows ? list_size() - rows : 0;
}

void SettingsForm::reveal()
{
    const size_t rows = visible_rows();
    if (selected_ < first_) {
        first_ = selected_;
    } else if (selected_ >= first_ + rows) {
        first_ = selected_ + 1 - rows;
    }
}

const std::string& SettingsForm::preview_family() const
{
    static const std::string none;
    return selected_ < shown_.size() ? shown_[selected_] : none;
}

double SettingsForm::preview_size() const
{
    if (const auto size = wm::parse_font_size(pick_size_.text())) {
        return *size;
    }
    return wm::parse_font_size(text(Field::Size)).value_or(12);
}

const char* SettingsForm::preview_text()
{
    return "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
}

std::string SettingsForm::pick_problem() const
{
    return wm::parse_font_size(pick_size_.text())
                   ? ""
                   : "The size must be a number from 4 to 96.";
}

void SettingsForm::refresh_styles()
{
    pick_styles_.clear();
    const std::string& family = preview_family();
    if (styles_of_ && !family.empty()) {
        auto found = styles_cache_.find(family);
        if (found == styles_cache_.end()) {
            found = styles_cache_.emplace(family, styles_of_(family)).first;
        }
        pick_styles_ = found->second;
    }
    // a style that the family does not have is the default one
    if (std::find(pick_styles_.begin(), pick_styles_.end(), pick_style_) ==
        pick_styles_.end()) {
        pick_style_.clear();
    }
}

bool SettingsForm::select(size_t index)
{
    if (list_size() == 0) {
        return false;
    }
    index = std::min(index, list_size() - 1);
    const size_t old_first = first_;
    const size_t old = selected_;
    selected_ = index;
    reveal();
    if (selected_ != old && view_ == View::Picker) {
        refresh_styles();
    }
    return selected_ != old || first_ != old_first;
}

void SettingsForm::search(const std::string& query)
{
    shown_.clear();
    for (const std::string& name : all_) {
        if (matches(name, query)) {
            shown_.push_back(name);
        }
    }
    selected_ = 0;
    first_ = 0;
    refresh_styles();
}

void SettingsForm::open_picker()
{
    view_ = View::Picker;
    pane_ = Pane::Search;
    pick_size_.set_text(text(Field::Size));
    pick_style_ = text(Field::Style);
    search_.set_text("");
    search("");
    // the family of the font field is where the list starts
    const std::string current(trim(text(Field::Font)));
    const auto found = std::find_if(shown_.begin(), shown_.end(),
                                    [&](const std::string& name) {
                                        return equal_names(name, current);
                                    });
    if (found != shown_.end()) {
        select(static_cast<size_t>(found - shown_.begin()));
    }
    // the style of the form belongs to this family, so it is kept
    pick_style_ = text(Field::Style);
    refresh_styles();
}

bool SettingsForm::choose()
{
    if (!pick_problem().empty()) {
        return false;
    }
    if (!shown_.empty()) {
        field_ref(Field::Font).set_text(shown_[selected_]);
        field_ref(Field::Size).set_text(std::string(trim(pick_size_.text())));
        field_ref(Field::Style).set_text(pick_style_);
        status_.clear();
    }
    close_picker();
    return true;
}

void SettingsForm::close_picker()
{
    view_ = View::Form;
    section_ = Section::Fonts; // the picker is opened from there
    focus_ = Field::Font;
    pressed_ = NoTarget;
}

std::string SettingsForm::programs_summary() const
{
    if (programs_.empty()) {
        return "None found";
    }
    const auto on = std::count_if(programs_.begin(), programs_.end(),
                                  [](const Program& p) { return p.enabled; });
    return std::to_string(on) + " on, " +
           std::to_string(static_cast<long>(programs_.size()) - on) + " off";
}

void SettingsForm::open_programs()
{
    view_ = View::Programs;
    programs_before_ = programs_;
    selected_ = 0;
    first_ = 0;
}

void SettingsForm::close_programs(bool keep)
{
    if (!keep) {
        programs_ = programs_before_;
    }
    view_ = View::Form;
    section_ = Section::Session; // the list is opened from there
    focus_ = Field::SessionPrograms;
    pressed_ = NoTarget;
}

size_t SettingsForm::list_size() const
{
    return view_ == View::Programs ? programs_.size() : shown_.size();
}

//! Switch the selected program; false when it can't be switched
bool SettingsForm::switch_program()
{
    if (selected_ >= programs_.size() || !programs_[selected_].reason.empty()) {
        return false;
    }
    programs_[selected_].enabled = !programs_[selected_].enabled;
    return true;
}

bool SettingsForm::programs_key(Key pressed)
{
    switch (pressed) {
        case Key::Return:
            close_programs(true);
            return true;
        case Key::Escape:
            close_programs(false);
            return true;
        default:
            return list_key(pressed);
    }
}

// The input

size_t SettingsForm::target_at(int x, int y) const
{
    if (view_ == View::Programs) {
        if (inside(cancel_rect(), x, y)) {
            return CancelButton;
        }
        if (inside(save_rect(), x, y)) {
            return SaveButton;
        }
        const int bottom = message_rect().y - metrics_.row_gap;
        if (y < rows_top() || y >= bottom) {
            return NoTarget;
        }
        const size_t row = first_ + static_cast<size_t>((y - rows_top()) /
                                                        metrics_.row_height);
        return row < programs_.size() && row < first_ + visible_rows()
                       ? FirstRow + row
                       : NoTarget;
    }
    if (view_ == View::Picker) {
        if (inside(search_rect(), x, y)) {
            return SearchBox;
        }
        if (inside(size_rect(), x, y)) {
            return SizeBox;
        }
        if (inside(style_rect(), x, y)) {
            return StyleBox;
        }
        if (inside(cancel_rect(), x, y)) {
            return CancelButton;
        }
        if (inside(save_rect(), x, y)) {
            return SaveButton;
        }
        if (y < rows_top()) {
            return NoTarget;
        }
        const size_t row = first_ + static_cast<size_t>((y - rows_top()) /
                                                        metrics_.row_height);
        // the strip under the last whole row shows no row
        return row < shown_.size() && row < first_ + visible_rows() &&
                               y < preview_rect().y - metrics_.row_gap
                       ? FirstRow + row
                       : NoTarget;
    }
    for (size_t i = 0; i < section_count; ++i) {
        if (inside(section_rect(static_cast<Section>(i)), x, y)) {
            return SectionAppearance + i;
        }
    }
    for (const Field f : section_fields(section_)) {
        if (inside(field_rect(f), x, y)) {
            return FontField + static_cast<size_t>(f);
        }
    }
    if (section_ == Section::Fonts && inside(choose_rect(), x, y)) {
        return ChooseButton;
    }
    if (inside(cancel_rect(), x, y)) {
        return CancelButton;
    }
    return inside(save_rect(), x, y) ? SaveButton : NoTarget;
}

void SettingsForm::press(int x, int y)
{
    pressed_ = target_at(x, y);
}

bool SettingsForm::release(int x, int y)
{
    const size_t target = target_at(x, y);
    const size_t pressed = pressed_;
    pressed_ = NoTarget;
    return target != NoTarget && target == pressed && act(target, x);
}

void SettingsForm::put_cursor(TextField& field, const Rect& rect, int x)
{
    const int room = rect.width - 2 * metrics_.padding;
    const int start = rect.x + metrics_.padding + shift(field, room);
    field.set_cursor(field.offset_at(x - start, measure_));
}

bool SettingsForm::act(size_t target, int x)
{
    switch (target) {
        case FontField:
        case SizeField:
        case StyleField:
        case TerminalField:
        case ScaleField:
        case AntialiasField:
        case HintingField:
        case RgbaField:
        case WallpaperField:
        case WallpaperModeField:
        case ClockFormatField:
        case DockField:
        case PrimaryMonitorField:
        case SecondaryMonitorField:
        case MonitorPlacementField:
        case LockDelayField:
        case DisplayOffField:
        case DisplayDpmsField:
        case ThemeField:
        case PanelStyleField:
        case DockStyleField:
        case LockStyleField:
        case LauncherStyleField:
        case SessionAutostartField:
        case SessionConfirmField: {
            focus_ = fields[target - FontField];
            if (is_choice(focus_)) {
                // the left half goes back, the right half goes on
                const Rect r = field_rect(focus_);
                step_choice(focus_, x < r.x + r.width / 2 ? -1 : 1);
                return true;
            }
            put_cursor(field_ref(focus_), field_rect(focus_), x);
            return true;
        }
        case SessionProgramsField:
            focus_ = Field::SessionPrograms;
            open_programs();
            return true;
        case ChooseButton:
            open_picker();
            return true;
        case SectionAppearance:
        case SectionFonts:
        case SectionDesktop:
        case SectionDisplays:
        case SectionPower:
        case SectionSecurity:
        case SectionApplications:
        case SectionSession:
            set_section(static_cast<Section>(target - SectionAppearance));
            return true;
        case CancelButton:
            if (view_ == View::Programs) {
                close_programs(false);
            } else if (view_ == View::Picker) {
                close_picker();
            } else {
                result_ = Result::Cancelled;
            }
            return true;
        case SaveButton:
            if (view_ == View::Programs) {
                close_programs(true);
                return true;
            }
            return view_ == View::Picker ? choose() : request_save();
        case SearchBox:
            pane_ = Pane::Search;
            put_cursor(search_, search_rect(), x);
            return true;
        case SizeBox:
            pane_ = Pane::Size;
            put_cursor(pick_size_, size_rect(), x);
            return true;
        case StyleBox:
            pane_ = Pane::Style;
            return cycle_style(1);
        default:
            break;
    }
    // a row of the list
    if (view_ == View::Programs) {
        select(target - FirstRow);
        switch_program();
        return true;
    }
    pane_ = Pane::Search;
    select(target - FirstRow);
    return true;
}

bool SettingsForm::request_save()
{
    for (const Field f : fields) {
        if (!problem(f).empty()) {
            // show the field that is wrong; the message tells why
            const Section was = section_;
            const Field before = focus_;
            section_ = section_of(f);
            focus_ = f;
            return section_ != was || focus_ != before;
        }
    }
    result_ = Result::Saved;
    status_.clear();
    return true;
}

bool SettingsForm::wheel(int notches)
{
    if (view_ == View::Form) {
        return false;
    }
    const auto step = static_cast<long>(notches) * metrics_.scroll_step;
    const auto target = std::clamp(static_cast<long>(first_) + step, 0L,
                                   static_cast<long>(max_first()));
    const bool changed = static_cast<size_t>(target) != first_;
    first_ = static_cast<size_t>(target);
    return changed;
}

//! The keys that edit a field; true when the field or the cursor changed
bool SettingsForm::edit(TextField& field, Key pressed)
{
    bool changed = false;
    switch (pressed) {
        case Key::Left:
            changed = field.left();
            break;
        case Key::Right:
            changed = field.right();
            break;
        case Key::Home:
            changed = field.home();
            break;
        case Key::End:
            changed = field.end();
            break;
        case Key::Back:
            changed = field.backspace();
            break;
        case Key::Delete:
            changed = field.erase();
            break;
        default:
            return false;
    }
    if (changed && (pressed == Key::Back || pressed == Key::Delete)) {
        status_.clear();
    }
    return changed;
}

//! The next (or the previous) value of a choice; the first one is the default
void SettingsForm::step_choice(Field field, int direction)
{
    const auto options = options_of(field);
    const auto n = static_cast<long>(options.size());
    const auto found = std::find_if(
            options.begin(), options.end(),
            [&](const auto& option) { return text(field) == option.first; });
    const long at = found == options.end() ? 0 : found - options.begin();
    field_ref(field).set_text(
            options[static_cast<size_t>((at + direction + n) % n)].first);
    status_.clear();
}

bool SettingsForm::key(Key pressed)
{
    switch (view_) {
        case View::Picker:
            return picker_key(pressed);
        case View::Programs:
            return programs_key(pressed);
        case View::Form:
            break;
    }
    return form_key(pressed);
}

bool SettingsForm::form_key(Key pressed)
{
    // the fields of the section that shows are the ones to go through
    const std::vector<Field>& list = section_fields(section_);
    const auto place = static_cast<size_t>(
            std::find(list.begin(), list.end(), focus_) - list.begin());
    const auto at = [&](size_t index) { return list[index % list.size()]; };
    switch (pressed) {
        case Key::Tab:
        case Key::Down:
            // like a combo box: the list of fonts opens from the font field
            if (pressed == Key::Down && focus_ == Field::Font) {
                open_picker();
                return true;
            }
            focus_ = at(place + 1);
            return true;
        case Key::BackTab:
        case Key::Up:
            focus_ = at(place + list.size() - 1);
            return true;
        case Key::NextSection:
        case Key::PrevSection: {
            const auto now = static_cast<size_t>(section_);
            set_section(static_cast<Section>(
                    (now +
                     (pressed == Key::NextSection ? 1 : section_count - 1)) %
                    section_count));
            return true;
        }
        case Key::Return:
            if (focus_ == Field::SessionPrograms) {
                open_programs();
                return true;
            }
            return request_save();
        case Key::Escape:
            result_ = Result::Cancelled;
            return true;
        case Key::PageUp:
        case Key::PageDown:
            return false;
        case Key::Left:
        case Key::Right:
        case Key::Home:
        case Key::End:
        case Key::Back:
        case Key::Delete:
            break;
    }
    if (focus_ == Field::SessionPrograms) {
        return false; // there is no text to edit
    }
    if (is_choice(focus_)) {
        switch (pressed) {
            case Key::Left:
            case Key::Right:
                step_choice(focus_, pressed == Key::Left ? -1 : 1);
                return true;
            case Key::Back:
            case Key::Delete: { // back to the default
                const bool set = !text(focus_).empty();
                field_ref(focus_).set_text("");
                status_.clear();
                return set;
            }
            default:
                return false;
        }
    }
    return edit(field_ref(focus_), pressed);
}

//! The keys that move in the list of fonts
bool SettingsForm::list_key(Key pressed)
{
    const size_t page = visible_rows();
    switch (pressed) {
        case Key::Up:
            return select(selected_ > 0 ? selected_ - 1 : 0);
        case Key::Down:
            return select(selected_ + 1);
        case Key::PageUp:
            return select(selected_ > page ? selected_ - page : 0);
        case Key::PageDown:
            return select(selected_ + page);
        case Key::Home:
            return select(0);
        case Key::End:
            return select(list_size());
        default:
            return false;
    }
}

bool SettingsForm::size_step(int direction)
{
    const double size = preview_size() + direction;
    const double limited =
            std::clamp(size, wm::min_font_size, wm::max_font_size);
    const std::string text = size_text(limited);
    const bool changed = text != pick_size_.text();
    pick_size_.set_text(text);
    return changed;
}

bool SettingsForm::cycle_style(int direction)
{
    // the default style is the first of the choices, then the styles
    std::vector<std::string> choices = {""};
    choices.insert(choices.end(), pick_styles_.begin(), pick_styles_.end());
    const auto found = std::find(choices.begin(), choices.end(), pick_style_);
    const auto n = static_cast<long>(choices.size());
    const long at = found - choices.begin();
    const auto to = static_cast<size_t>((at + direction % n + n) % n);
    const bool changed = choices[to] != pick_style_;
    pick_style_ = choices[to];
    return changed;
}

bool SettingsForm::picker_key(Key pressed)
{
    switch (pressed) {
        case Key::Return:
            choose();
            return true;
        case Key::Escape:
            if (!search_.text().empty()) {
                search_.set_text(""); // the first Escape clears the search
                search("");
            } else {
                close_picker();
            }
            return true;
        case Key::Tab:
        case Key::BackTab: {
            const int step = pressed == Key::Tab ? 1 : 2;
            pane_ = static_cast<Pane>((static_cast<int>(pane_) + step) % 3);
            return true;
        }
        default:
            break;
    }
    switch (pane_) {
        case Pane::Search: {
            if (list_key(pressed)) {
                return true;
            }
            const std::string before = search_.text();
            const bool changed = edit(search_, pressed);
            if (before != search_.text()) {
                search(search_.text());
            }
            return changed;
        }
        case Pane::Size:
            if (pressed == Key::Up || pressed == Key::Down) {
                return size_step(pressed == Key::Up ? 1 : -1);
            }
            if (pressed == Key::PageUp || pressed == Key::PageDown) {
                return list_key(pressed);
            }
            return edit(pick_size_, pressed);
        case Pane::Style:
            switch (pressed) {
                case Key::Up:
                case Key::Left:
                    return cycle_style(-1);
                case Key::Down:
                case Key::Right:
                    return cycle_style(1);
                default:
                    return list_key(pressed);
            }
    }
    return false;
}

bool SettingsForm::type(const std::string& typed)
{
    std::string text = printable(typed);
    if (text.empty()) {
        return false;
    }
    if (view_ == View::Programs) {
        return text == " " && switch_program();
    }
    if (view_ == View::Form) {
        if (focus_ == Field::SessionPrograms) {
            if (text != " ") {
                return false;
            }
            open_programs();
            return true;
        }
        status_.clear();
        if (is_choice(focus_)) { // Space goes to the next value
            if (text != " ") {
                return false;
            }
            step_choice(focus_, 1);
            return true;
        }
        return field_ref(focus_).insert(text);
    }
    switch (pane_) {
        case Pane::Search:
            search_.insert(text);
            search(search_.text());
            return true;
        case Pane::Size:
            // a number: the other characters are not wanted
            text.erase(std::remove_if(
                               text.begin(), text.end(),
                               [](char c) {
                                   return std::isdigit(
                                                  static_cast<unsigned char>(
                                                          c)) == 0 &&
                                          c != '.';
                               }),
                       text.end());
            return pick_size_.insert(text);
        case Pane::Style:
            break;
    }
    return false;
}

} // namespace tiny::app
