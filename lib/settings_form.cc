#include "settings_form.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <utility>

#include "ini.h"
#include "style.h"
#include "wm/config.h"
#include "xdg/desktop_entry.h"

namespace tiny {

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

//! The text without its last character (UTF-8 can take more bytes)
void erase_last_character(std::string& text)
{
    while (!text.empty()) {
        const auto last = static_cast<unsigned char>(text.back());
        text.pop_back();
        if ((last & 0xC0) != 0x80) {
            break;
        }
    }
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

constexpr SettingsForm::Field fields[] = {SettingsForm::Field::Font,
                                          SettingsForm::Field::Terminal,
                                          SettingsForm::Field::Scale};

} // namespace

SettingsForm::Metrics SettingsForm::Metrics::scaled(double factor)
{
    Metrics m;
    m.margin = tiny::scaled(m.margin, factor);
    m.label_width = tiny::scaled(m.label_width, factor);
    m.row_height = tiny::scaled(m.row_height, factor);
    m.row_gap = tiny::scaled(m.row_gap, factor);
    m.button_width = tiny::scaled(m.button_width, factor);
    m.choose_width = tiny::scaled(m.choose_width, factor);
    m.text_size = tiny::scaled(m.text_size, factor);
    return m;
}

SettingsForm::SettingsForm(Values initial, std::vector<std::string> families,
                           const Metrics& metrics):
    values_(std::move(initial)),
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
    // three fields, the message of two rows and the buttons
    const int rows = 3 + 2 + 1;
    return 2 * metrics_.margin + rows * metrics_.row_height +
           4 * metrics_.row_gap;
}

const std::string& SettingsForm::text(Field field) const
{
    switch (field) {
        case Field::Font:
            return values_.font;
        case Field::Terminal:
            return values_.terminal;
        case Field::Scale:
            break;
    }
    return values_.scale;
}

std::string SettingsForm::problem(Field field) const
{
    switch (field) {
        case Field::Font:
            return wm::valid_font(values_.font)
                           ? ""
                           : "The font must be the name of a font family.";
        case Field::Terminal:
            return xdg::split_exec(values_.terminal).empty()
                           ? "The terminal must be a program with arguments."
                           : "";
        case Field::Scale:
            return wm::parse_scale(values_.scale)
                           ? ""
                           : "The scale must be a number above 0, up to 16.";
    }
    return "";
}

std::string SettingsForm::problem() const
{
    for (const Field field : fields) {
        std::string what = problem(field);
        if (!what.empty()) {
            return what;
        }
    }
    return "";
}

SettingsForm::Values SettingsForm::clean_values() const
{
    const auto clean = [](const std::string& text) {
        return std::string(trim(text));
    };
    return {clean(values_.font), clean(values_.terminal), clean(values_.scale)};
}

void SettingsForm::reject(const std::string& message)
{
    result_ = Result::Editing;
    status_ = message;
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
    const Rect last = label_rect(Field::Scale);
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

// The list of fonts: the search field, then the rows

int SettingsForm::rows_top() const
{
    return 2 * metrics_.margin + metrics_.row_height;
}

Rect SettingsForm::search_rect() const
{
    return {metrics_.margin, metrics_.margin,
            std::max(1, width_ - 2 * metrics_.margin), metrics_.row_height};
}

size_t SettingsForm::visible_rows() const
{
    const int rows = (height_ - rows_top()) / metrics_.row_height;
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
    return selected_ != old || first_ != old_first;
}

void SettingsForm::search(const std::string& query)
{
    query_ = query;
    shown_.clear();
    for (const std::string& name : all_) {
        if (matches(name, query_)) {
            shown_.push_back(name);
        }
    }
    selected_ = 0;
    first_ = 0;
}

void SettingsForm::open_picker()
{
    view_ = View::Picker;
    search("");
    // the family of the font field is where the list starts
    const std::string current(trim(values_.font));
    const auto found = std::find_if(shown_.begin(), shown_.end(),
                                    [&](const std::string& name) {
                                        return equal_names(name, current);
                                    });
    if (found != shown_.end()) {
        select(static_cast<size_t>(found - shown_.begin()));
    }
}

void SettingsForm::choose()
{
    if (!shown_.empty()) {
        values_.font = shown_[selected_];
        status_.clear();
    }
    close_picker();
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
        if (y < rows_top()) {
            return NoTarget;
        }
        const size_t row = first_ + static_cast<size_t>((y - rows_top()) /
                                                        metrics_.row_height);
        // the strip under the last whole row shows no row
        return row < shown_.size() && row < first_ + visible_rows()
                       ? FirstRow + row
                       : NoTarget;
    }
    if (inside(field_rect(Field::Font), x, y)) {
        return FontField;
    }
    if (inside(field_rect(Field::Terminal), x, y)) {
        return TerminalField;
    }
    if (inside(field_rect(Field::Scale), x, y)) {
        return ScaleField;
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
    return target != NoTarget && target == pressed && act(target);
}

bool SettingsForm::act(size_t target)
{
    switch (target) {
        case FontField:
            focus_ = Field::Font;
            return true;
        case TerminalField:
            focus_ = Field::Terminal;
            return true;
        case ScaleField:
            focus_ = Field::Scale;
            return true;
        case ChooseButton:
            open_picker();
            return true;
        case CancelButton:
            result_ = Result::Cancelled;
            return true;
        case SaveButton:
            return request_save();
        default:
            break;
    }
    // a row of the list
    select(target - FirstRow);
    choose();
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

bool SettingsForm::key(Key pressed)
{
    if (view_ == View::Picker) {
        return picker_key(pressed);
    }
    const auto count = std::size(fields);
    const auto index = static_cast<size_t>(focus_);
    switch (pressed) {
        case Key::Tab:
        case Key::BackTab:
            focus_ = fields[pressed == Key::Tab ? (index + 1) % count
                                                : (index + count - 1) % count];
            return true;
        case Key::Down:
            // like a combo box: the list of fonts opens from the font field
            if (focus_ == Field::Font) {
                open_picker();
                return true;
            }
            focus_ = fields[(index + 1) % count];
            return true;
        case Key::Up:
            focus_ = fields[(index + count - 1) % count];
            return true;
        case Key::Back:
            return edit("");
        case Key::Return:
            return request_save();
        case Key::Escape:
            result_ = Result::Cancelled;
            return true;
        case Key::PageUp:
        case Key::PageDown:
        case Key::Home:
        case Key::End:
            break;
    }
    return false;
}

bool SettingsForm::picker_key(Key pressed)
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
        case Key::Return:
            choose();
            return true;
        case Key::Back:
            if (query_.empty()) {
                return false;
            }
            {
                std::string shorter = query_;
                erase_last_character(shorter);
                search(shorter);
            }
            return true;
        case Key::Escape:
            if (!query_.empty()) {
                search(""); // the first Escape clears the search
            } else {
                close_picker();
            }
            return true;
        case Key::Tab:
        case Key::BackTab:
            break;
    }
    return false;
}

std::string& SettingsForm::focused_text()
{
    switch (focus_) {
        case Field::Font:
            return values_.font;
        case Field::Terminal:
            return values_.terminal;
        case Field::Scale:
            break;
    }
    return values_.scale;
}

bool SettingsForm::edit(const std::string& added)
{
    std::string& target = focused_text();
    if (added.empty()) { // a backspace
        if (target.empty()) {
            return false;
        }
        erase_last_character(target);
    } else {
        target += added;
    }
    status_.clear();
    return true;
}

bool SettingsForm::type(const std::string& typed)
{
    const std::string text = printable(typed);
    if (text.empty()) {
        return false;
    }
    if (view_ == View::Picker) {
        search(query_ + text);
        return true;
    }
    return edit(text);
}

} // namespace tiny
