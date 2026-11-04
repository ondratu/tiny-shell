#include "app/font_browser.h"

#include <algorithm>

#include "style.h"

namespace tiny::app {

namespace {

//! The sizes of the sentences of the specimen at the scale 1
constexpr int sentence_sizes[] = {8, 12, 16, 20, 24, 32, 64, 128};

//! Not a cell and not a button
constexpr size_t nothing = static_cast<size_t>(-1);

//! The header button back; no cell has this index
constexpr size_t back_button = static_cast<size_t>(-2);

bool inside(const Rect& r, int x, int y)
{
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

} // namespace

FontBrowser::Metrics FontBrowser::Metrics::scaled(double factor)
{
    Metrics m;
    m.cell_width = tiny::scaled(m.cell_width, factor);
    m.cell_height = tiny::scaled(m.cell_height, factor);
    m.margin = tiny::scaled(m.margin, factor);
    m.preview = tiny::scaled(m.preview, factor);
    m.preview_baseline = tiny::scaled(m.preview_baseline, factor);
    m.name_size = tiny::scaled(m.name_size, factor);
    m.name_baseline = tiny::scaled(m.name_baseline, factor);
    m.header = tiny::scaled(m.header, factor);
    m.back_width = tiny::scaled(m.back_width, factor);
    m.charset_size = tiny::scaled(m.charset_size, factor);
    m.line_gap = tiny::scaled(m.line_gap, factor);
    m.small_size = tiny::scaled(m.small_size, factor);
    return m;
}

std::vector<FontBrowser::Line> FontBrowser::specimen(const Metrics& metrics)
{
    std::vector<Line> lines = {
            {"abcdefghijklmnopqrstuvwxyz", metrics.charset_size},
            {"ABCDEFGHIJKLMNOPQRSTUVWXYZ", metrics.charset_size},
            {"0123456789 !?@#$%^&*()[]{}<>;:'\",./\\|-_=+~`",
             metrics.charset_size}};
    // the sizes of the sentences go with the smallest one
    for (const int size : sentence_sizes) {
        lines.push_back({"Lorem ipsum dolor sit amet, consectetur adipiscing "
                         "elit.",
                         size * metrics.small_size / sentence_sizes[0]});
    }
    return lines;
}

FontBrowser::FontBrowser(std::vector<std::string> families,
                         const Metrics& metrics):
    families_(std::move(families)), metrics_(metrics), pressed_(nothing)
{
    metrics_.cell_width = std::max(1, metrics_.cell_width);
    metrics_.cell_height = std::max(1, metrics_.cell_height);
}

const std::string& FontBrowser::family() const
{
    static const std::string none;
    return selected_ < families_.size() ? families_[selected_] : none;
}

int FontBrowser::content_height() const
{
    if (view_ == View::Detail) {
        return detail_height_;
    }
    const auto count = static_cast<int>(families_.size());
    const int rows = (count + static_cast<int>(columns_) - 1) /
                     static_cast<int>(columns_);
    return 2 * metrics_.margin + rows * metrics_.cell_height;
}

int FontBrowser::view_height() const
{
    return std::max(0, height_ - top());
}

int FontBrowser::max_scroll() const
{
    return std::max(0, content_height() - view_height());
}

bool FontBrowser::scroll_to(int position)
{
    const int old = scroll_;
    scroll_ = std::clamp(position, 0, max_scroll());
    return scroll_ != old;
}

int FontBrowser::step() const
{
    return std::max(1, metrics_.cell_height / 3);
}

int FontBrowser::grid_left() const
{
    return std::max(
            0, (width_ - static_cast<int>(columns_) * metrics_.cell_width) / 2);
}

void FontBrowser::resize(int width, int height)
{
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    columns_ = static_cast<size_t>(
            std::max(1, (width_ - 2 * metrics_.margin) / metrics_.cell_width));
    scroll_to(scroll_);
}

void FontBrowser::set_detail_height(int height)
{
    detail_height_ = std::max(0, height);
    if (view_ == View::Detail) {
        scroll_to(scroll_);
    }
}

Rect FontBrowser::cell_rect(size_t index) const
{
    const auto col = static_cast<int>(index % columns_);
    const auto row = static_cast<int>(index / columns_);
    return {grid_left() + col * metrics_.cell_width,
            metrics_.margin + row * metrics_.cell_height - scroll_,
            metrics_.cell_width, metrics_.cell_height};
}

std::pair<size_t, size_t> FontBrowser::visible_cells() const
{
    if (view_ != View::Overview || families_.empty()) {
        return {0, 0};
    }
    const int first_row =
            std::max(0, scroll_ - metrics_.margin) / metrics_.cell_height;
    const int end_row =
            (scroll_ + height_ - metrics_.margin + metrics_.cell_height - 1) /
            metrics_.cell_height;
    const size_t first = static_cast<size_t>(first_row) * columns_;
    const size_t end = static_cast<size_t>(std::max(end_row, 0)) * columns_;
    return {std::min(first, families_.size()), std::min(end, families_.size())};
}

size_t FontBrowser::cell_at(int x, int y) const
{
    const int column_x = x - grid_left();
    const int row_y = y + scroll_ - metrics_.margin;
    if (view_ != View::Overview || column_x < 0 || row_y < 0) {
        return families_.size();
    }
    const auto col = static_cast<size_t>(column_x / metrics_.cell_width);
    const auto row = static_cast<size_t>(row_y / metrics_.cell_height);
    const size_t index = row * columns_ + col;
    return col < columns_ && index < families_.size() ? index
                                                      : families_.size();
}

Rect FontBrowser::back_rect() const
{
    return {0, 0, metrics_.back_width, metrics_.header};
}

size_t FontBrowser::target_at(int x, int y) const
{
    if (view_ == View::Detail) {
        return inside(back_rect(), x, y) ? back_button : nothing;
    }
    const size_t index = cell_at(x, y);
    return index < families_.size() ? index : nothing;
}

void FontBrowser::press(int x, int y)
{
    pressed_ = target_at(x, y);
}

bool FontBrowser::release(int x, int y)
{
    const size_t pressed = std::exchange(pressed_, nothing);
    if (pressed == nothing || pressed != target_at(x, y)) {
        return false;
    }
    if (pressed == back_button) {
        back();
    } else {
        open(pressed);
    }
    return true;
}

bool FontBrowser::wheel(int notches)
{
    return scroll_to(scroll_ + notches * step());
}

bool FontBrowser::move_selection(size_t index)
{
    if (families_.empty()) {
        return false;
    }
    index = std::min(index, families_.size() - 1);
    const Rect cell = cell_rect(index);
    // the cell is brought into view with its margin, the first row to the top
    const int top_edge = cell.y + scroll_ - metrics_.margin;
    const int bottom_edge = cell.y + scroll_ + cell.height + metrics_.margin;
    if (top_edge < scroll_) {
        scroll_to(top_edge);
    } else if (bottom_edge > scroll_ + view_height()) {
        scroll_to(bottom_edge - view_height());
    }
    const bool changed = index != selected_;
    selected_ = index;
    return changed;
}

bool FontBrowser::key(Key key)
{
    if (view_ == View::Detail) {
        const int page = std::max(1, view_height() - step());
        switch (key) {
            case Key::Up:
                return scroll_to(scroll_ - step());
            case Key::Down:
                return scroll_to(scroll_ + step());
            case Key::PageUp:
                return scroll_to(scroll_ - page);
            case Key::PageDown:
                return scroll_to(scroll_ + page);
            case Key::Home:
                return scroll_to(0);
            case Key::End:
                return scroll_to(max_scroll());
            case Key::Back:
            case Key::Escape:
                back();
                return true;
            case Key::Left:
            case Key::Right:
            case Key::Return:
                return false;
        }
        return false;
    }

    const size_t columns = columns_;
    const size_t page = std::max<size_t>(
            1, static_cast<size_t>(
                       std::max(1, view_height() / metrics_.cell_height)) *
                       columns);
    switch (key) {
        case Key::Left:
            return move_selection(selected_ > 0 ? selected_ - 1 : 0);
        case Key::Right:
            return move_selection(selected_ + 1);
        case Key::Up:
            return move_selection(selected_ >= columns ? selected_ - columns
                                                       : selected_);
        case Key::Down:
            // the last row may be shorter: stay when there is no cell below
            return move_selection(selected_ + columns < families_.size()
                                          ? selected_ + columns
                                          : selected_);
        case Key::PageUp:
            return move_selection(selected_ > page ? selected_ - page : 0);
        case Key::PageDown:
            return move_selection(selected_ + page);
        case Key::Home:
            return move_selection(0);
        case Key::End:
            return move_selection(families_.size());
        case Key::Return:
            if (families_.empty()) {
                return false;
            }
            open(selected_);
            return true;
        case Key::Back:
        case Key::Escape:
            return false;
    }
    return false;
}

void FontBrowser::open(size_t index)
{
    if (index >= families_.size()) {
        return;
    }
    selected_ = index;
    overview_scroll_ = scroll_;
    view_ = View::Detail;
    detail_height_ = 0; // unknown until the owner measures the new family
    scroll_ = 0;
}

void FontBrowser::back()
{
    if (view_ == View::Overview) {
        return;
    }
    view_ = View::Overview;
    scroll_ = 0;
    scroll_to(overview_scroll_);
    // a resize in the detail may have moved the cell
    move_selection(selected_);
}

} // namespace tiny::app
