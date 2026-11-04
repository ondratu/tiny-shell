#include "text_field.h"

#include <algorithm>
#include <utility>

namespace tiny {

namespace {

//! A byte in the middle of a UTF-8 character
bool is_continuation(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0) == 0x80;
}

} // namespace

TextField::TextField(std::string text): text_(std::move(text))
{
    cursor_ = text_.size();
}

void TextField::set_text(std::string text)
{
    text_ = std::move(text);
    cursor_ = text_.size();
}

size_t TextField::previous(size_t offset) const
{
    while (offset > 0) {
        --offset;
        if (!is_continuation(text_[offset])) {
            break;
        }
    }
    return offset;
}

size_t TextField::next(size_t offset) const
{
    while (offset < text_.size()) {
        ++offset;
        if (offset == text_.size() || !is_continuation(text_[offset])) {
            break;
        }
    }
    return offset;
}

bool TextField::insert(const std::string& added)
{
    if (added.empty()) {
        return false;
    }
    text_.insert(cursor_, added);
    cursor_ += added.size();
    return true;
}

bool TextField::backspace()
{
    if (cursor_ == 0) {
        return false;
    }
    const size_t start = previous(cursor_);
    text_.erase(start, cursor_ - start);
    cursor_ = start;
    return true;
}

bool TextField::erase()
{
    if (cursor_ == text_.size()) {
        return false;
    }
    text_.erase(cursor_, next(cursor_) - cursor_);
    return true;
}

bool TextField::left()
{
    if (cursor_ == 0) {
        return false;
    }
    cursor_ = previous(cursor_);
    return true;
}

bool TextField::right()
{
    if (cursor_ == text_.size()) {
        return false;
    }
    cursor_ = next(cursor_);
    return true;
}

bool TextField::home()
{
    const bool moved = cursor_ != 0;
    cursor_ = 0;
    return moved;
}

bool TextField::end()
{
    const bool moved = cursor_ != text_.size();
    cursor_ = text_.size();
    return moved;
}

void TextField::set_cursor(size_t offset)
{
    offset = std::min(offset, text_.size());
    while (offset > 0 && offset < text_.size() &&
           is_continuation(text_[offset])) {
        --offset;
    }
    cursor_ = offset;
}

size_t TextField::offset_at(int x, const Measure& measure) const
{
    if (!measure) {
        return text_.size();
    }
    // the border before which the pointer is nearer than to the next one
    size_t offset = 0;
    int left = 0;
    while (offset < text_.size()) {
        const size_t following = next(offset);
        const int right = measure(text_.substr(0, following));
        if (x < left + (right - left) / 2) {
            break;
        }
        left = right;
        offset = following;
    }
    return offset;
}

} // namespace tiny
