#include "xdg/font_list.h"

#include <fontconfig/fontconfig.h>

#include <algorithm>
#include <cctype>
#include <memory>

namespace tiny::xdg {

namespace {

//! Compares ASCII letters without their case; the exact spelling breaks a tie
bool name_less(const std::string& a, const std::string& b)
{
    const size_t common = std::min(a.size(), b.size());
    for (size_t i = 0; i < common; ++i) {
        const int ca = std::tolower(static_cast<unsigned char>(a[i]));
        const int cb = std::tolower(static_cast<unsigned char>(b[i]));
        if (ca != cb) {
            return ca < cb;
        }
    }
    return a.size() != b.size() ? a.size() < b.size() : a < b;
}

bool is_sample_char(FcChar32 c)
{
    // a space, a control character, a surrogate, a code that is not a character
    const bool skipped = c <= 0x20 || (c >= 0x7f && c <= 0xa0) || c == 0xad ||
                         (c >= 0xd800 && c <= 0xdfff) ||
                         (c >= 0xfdd0 && c <= 0xfdef) ||
                         (c & 0xfffe) == 0xfffe || c > 0x10ffff;
    return !skipped;
}

void append_utf8(std::string& out, FcChar32 c)
{
    if (c < 0x80) {
        out += static_cast<char>(c);
    } else if (c < 0x800) {
        out += static_cast<char>(0xc0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 0x3f));
    } else if (c < 0x10000) {
        out += static_cast<char>(0xe0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (c & 0x3f));
    } else {
        out += static_cast<char>(0xf0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 0x3f));
        out += static_cast<char>(0x80 | ((c >> 6) & 0x3f));
        out += static_cast<char>(0x80 | (c & 0x3f));
    }
}

} // namespace

std::string font_sample(const FcCharSet* chars, size_t skip, size_t count)
{
    std::string sample;
    if (chars == nullptr) {
        return sample;
    }
    FcChar32 map[FC_CHARSET_MAP_SIZE];
    FcChar32 next = 0;
    for (FcChar32 base = FcCharSetFirstPage(chars, map, &next);
         base != FC_CHARSET_DONE; base = FcCharSetNextPage(chars, map, &next)) {
        for (FcChar32 i = 0; i < FC_CHARSET_MAP_SIZE * 32; ++i) {
            if ((map[i / 32] & (FcChar32{1} << (i % 32))) == 0 ||
                !is_sample_char(base + i)) {
                continue;
            }
            if (skip > 0) {
                --skip;
            } else if (count > 0) {
                append_utf8(sample, base + i);
                if (--count == 0) {
                    return sample;
                }
            }
        }
    }
    return sample;
}

std::vector<std::string> sort_families(std::vector<std::string> names)
{
    names.erase(std::remove_if(names.begin(), names.end(),
                               [](const std::string& name) {
                                   return name.empty() || name.front() == '.';
                               }),
                names.end());
    std::sort(names.begin(), names.end(), name_less);
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

std::vector<std::string> font_families()
{
    std::vector<std::string> names;
    const std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(
            FcPatternCreate(), &FcPatternDestroy);
    const std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> objects(
            FcObjectSetBuild(FC_FAMILY, nullptr), &FcObjectSetDestroy);
    if (!pattern || !objects) {
        return names;
    }
    const std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)> fonts(
            FcFontList(nullptr, pattern.get(), objects.get()),
            &FcFontSetDestroy);
    if (!fonts) {
        return names;
    }
    for (int i = 0; i < fonts->nfont; ++i) {
        FcChar8* family = nullptr;
        if (FcPatternGetString(fonts->fonts[i], FC_FAMILY, 0, &family) ==
            FcResultMatch) {
            names.emplace_back(reinterpret_cast<const char*>(family));
        }
    }
    return sort_families(std::move(names));
}

} // namespace tiny::xdg
