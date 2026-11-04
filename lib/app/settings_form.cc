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

constexpr SettingsForm::Field fields[] = {
        SettingsForm::Field::Font,    SettingsForm::Field::Size,
        SettingsForm::Field::Style,   SettingsForm::Field::Terminal,
        SettingsForm::Field::Scale,   SettingsForm::Field::Antialias,
        SettingsForm::Field::Hinting, SettingsForm::Field::Rgba};

constexpr size_t field_count = std::size(fields);

} // namespace

SettingsForm::Metrics SettingsForm::Metrics::scaled(double factor)
{
    Metrics m;
    m.margin = tiny::scaled(m.margin, factor);
    m.padding = tiny::scaled(m.padding, factor);
    m.label_width = tiny::scaled(m.label_width, factor);
    m.row_height = tiny::scaled(m.row_height, factor);
    m.row_gap = tiny::scaled(m.row_gap, factor);
    m.button_width = tiny::scaled(m.button_width, factor);
    m.choose_width = tiny::scaled(m.choose_width, factor);
    m.text_size = tiny::scaled(m.text_size, factor);
    return m;
}

SettingsForm::SettingsForm(const Values& initial,
                           std::vector<std::string> families,
                           const Metrics& metrics, StylesFn styles_of):
    fields_{TextField(initial.font),    TextField(initial.size),
            TextField(initial.style),   TextField(initial.terminal),
            TextField(initial.scale),   TextField(initial.antialias),
            TextField(initial.hinting), TextField(initial.rgba)},
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
    // the fields, the message of two rows and the buttons
    const auto rows = static_cast<int>(field_count) + 2 + 1;
    return 2 * metrics_.margin + rows * metrics_.row_height +
           static_cast<int>(field_count + 1) * metrics_.row_gap;
}

SettingsForm::Values SettingsForm::values() const
{
    return {text(Field::Font),    text(Field::Terminal), text(Field::Scale),
            text(Field::Size),    text(Field::Style),    text(Field::Antialias),
            text(Field::Hinting), text(Field::Rgba)};
}

bool SettingsForm::is_choice(Field field)
{
    return field >= Field::Antialias;
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
    for (const wm::XftChoice& c : wm::xft_choices(xft_option(field))) {
        if (text(field) == c.value) {
            return c.label;
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
    return {clean(v.font),    clean(v.terminal), clean(v.scale),
            clean(v.size),    clean(v.style),    clean(v.antialias),
            clean(v.hinting), clean(v.rgba)};
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

Rect SettingsForm::label_rect(Field field) const
{
    const int y =
            metrics_.margin +
            static_cast<int>(field) * (metrics_.row_height + metrics_.row_gap);
    return {metrics_.margin, y, metrics_.label_width, metrics_.row_height};
}

Rect SettingsForm::field_rect(Field field) const
{
    const Rect label = label_rect(field);
    const int x = metrics_.margin + metrics_.label_width;
    // the font has the button that opens the list at its end
    const int reserved =
            field == Field::Font ? metrics_.choose_width + metrics_.row_gap : 0;
    return {x, label.y, std::max(1, width_ - x - metrics_.margin - reserved),
            metrics_.row_height};
}

Rect SettingsForm::choose_rect() const
{
    const Rect label = label_rect(Field::Font);
    return {width_ - metrics_.margin - metrics_.choose_width, label.y,
            metrics_.choose_width, metrics_.row_height};
}

Rect SettingsForm::message_rect() const
{
    const Rect last = label_rect(Field::Rgba);
    return {metrics_.margin + metrics_.label_width,
            last.y + last.height + metrics_.row_gap,
            std::max(1, width_ - 2 * metrics_.margin - metrics_.label_width),
            2 * metrics_.row_height};
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
    const int bottom = preview_rect().y - metrics_.row_gap;
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
    return shown_.size() > rows ? shown_.size() - rows : 0;
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
    if (shown_.empty()) {
        return false;
    }
    index = std::min(index, shown_.size() - 1);
    const size_t old_first = first_;
    const size_t old = selected_;
    selected_ = index;
    reveal();
    if (selected_ != old) {
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
    focus_ = Field::Font;
    pressed_ = NoTarget;
}

// The input

size_t SettingsForm::target_at(int x, int y) const
{
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
    for (const Field f : fields) {
        if (inside(field_rect(f), x, y)) {
            return FontField + static_cast<size_t>(f);
        }
    }
    if (inside(choose_rect(), x, y)) {
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
        case RgbaField: {
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
        case ChooseButton:
            open_picker();
            return true;
        case CancelButton:
            if (view_ == View::Picker) {
                close_picker();
            } else {
                result_ = Result::Cancelled;
            }
            return true;
        case SaveButton:
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
    pane_ = Pane::Search;
    select(target - FirstRow);
    return true;
}

bool SettingsForm::request_save()
{
    if (!problem().empty()) {
        return false; // the message under the fields tells why
    }
    result_ = Result::Saved;
    status_.clear();
    return true;
}

bool SettingsForm::wheel(int notches)
{
    if (view_ != View::Picker) {
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
    const wm::XftChoices choices = wm::xft_choices(xft_option(field));
    const auto n = static_cast<long>(choices.size());
    const auto* const found = std::find_if(
            choices.begin(), choices.end(),
            [&](const wm::XftChoice& c) { return text(field) == c.value; });
    const long at = found == choices.end() ? 0 : found - choices.begin();
    field_ref(field).set_text(
            choices[static_cast<size_t>((at + direction + n) % n)].value);
    status_.clear();
}

bool SettingsForm::key(Key pressed)
{
    return view_ == View::Picker ? picker_key(pressed) : form_key(pressed);
}

bool SettingsForm::form_key(Key pressed)
{
    const auto index = static_cast<size_t>(focus_);
    switch (pressed) {
        case Key::Tab:
        case Key::BackTab:
            focus_ = fields[pressed == Key::Tab
                                    ? (index + 1) % field_count
                                    : (index + field_count - 1) % field_count];
            return true;
        case Key::Down:
            // like a combo box: the list of fonts opens from the font field
            if (focus_ == Field::Font) {
                open_picker();
                return true;
            }
            focus_ = fields[(index + 1) % field_count];
            return true;
        case Key::Up:
            focus_ = fields[(index + field_count - 1) % field_count];
            return true;
        case Key::Return:
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
            return select(shown_.size());
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
    if (view_ == View::Form) {
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
