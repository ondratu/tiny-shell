#include "xdg/font_info.h"

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H

#include <algorithm>
#include <cctype>
#include <memory>
#include <utility>

#include "xdg/font_list.h"

namespace tiny::xdg {

namespace {

using FaceSet = std::unique_ptr<FcFontSet, decltype(&FcFontSetDestroy)>;

void append_utf8(std::string& out, unsigned c)
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

//! The 16 bit unit at `i` of big endian text
unsigned unit(const std::string& bytes, size_t i)
{
    return (static_cast<unsigned>(static_cast<unsigned char>(bytes[i])) << 8) |
           static_cast<unsigned char>(bytes[i + 1]);
}

//! A name of the table that is English: Windows 0x?09, Macintosh 0, Unicode
bool is_english(const FT_SfntName& name)
{
    switch (name.platform_id) {
        case 3:
            return (name.language_id & 0xff) == 0x09;
        case 1:
            return name.language_id == 0;
        default:
            return false;
    }
}

std::string string_of(const FcPattern* pattern, const char* object, int n = 0)
{
    FcChar8* value = nullptr;
    if (FcPatternGetString(pattern, object, n, &value) == FcResultMatch &&
        value != nullptr) {
        return reinterpret_cast<const char*>(value);
    }
    return {};
}

//! All the values of a string property, without a repeated one
std::string strings_of(const FcPattern* pattern, const char* object)
{
    std::string all;
    std::vector<std::string> seen;
    FcChar8* value = nullptr;
    for (int i = 0;
         FcPatternGetString(pattern, object, i, &value) == FcResultMatch; ++i) {
        std::string text = reinterpret_cast<const char*>(value);
        if (std::find(seen.begin(), seen.end(), text) == seen.end()) {
            all += (all.empty() ? "" : ", ") + text;
            seen.push_back(std::move(text));
        }
    }
    return all;
}

const char* spacing_name(int spacing)
{
    switch (spacing) {
        case FC_MONO:
            return "Monospace";
        case FC_CHARCELL:
            return "Character cells";
        case FC_DUAL:
            return "Dual width";
        default:
            return "Proportional";
    }
}

std::string number_with_commas(int number)
{
    std::string digits = std::to_string(number);
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<size_t>(i), ",");
    }
    return digits;
}

//! The codes of the languages of a face, the first ones only
std::string languages_of(const FcPattern* pattern)
{
    constexpr int shown = 16;
    FcLangSet* langs = nullptr;
    if (FcPatternGetLangSet(pattern, FC_LANG, 0, &langs) != FcResultMatch ||
        langs == nullptr) {
        return {};
    }
    FcStrSet* set = FcLangSetGetLangs(langs);
    std::vector<std::string> codes;
    FcStrList* list = FcStrListCreate(set);
    FcStrListFirst(list);
    while (const FcChar8* code = FcStrListNext(list)) {
        codes.emplace_back(reinterpret_cast<const char*>(code));
    }
    FcStrListDone(list);
    FcStrSetDestroy(set);
    std::sort(codes.begin(), codes.end());

    std::string text;
    for (size_t i = 0; i < codes.size() && i < shown; ++i) {
        text += (i == 0 ? "" : ", ") + codes[i];
    }
    if (codes.size() > shown) {
        text += ", \xE2\x80\xA6 (" + std::to_string(codes.size()) + ")";
    }
    return text;
}

//! The faces of a family from fontconfig, with what the info needs
FaceSet faces_of(const std::string& family)
{
    FaceSet none(nullptr, &FcFontSetDestroy);
    const std::unique_ptr<FcPattern, decltype(&FcPatternDestroy)> pattern(
            FcPatternCreate(), &FcPatternDestroy);
    const std::unique_ptr<FcObjectSet, decltype(&FcObjectSetDestroy)> objects(
            FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, FC_INDEX, FC_FOUNDRY,
                             FC_FONTFORMAT, FC_SPACING, FC_LANG, FC_CHARSET,
                             FC_VARIABLE, FC_COLOR, FC_SCALABLE, FC_FULLNAME,
                             FC_POSTSCRIPT_NAME, nullptr),
            &FcObjectSetDestroy);
    if (!pattern || !objects ||
        !FcPatternAddString(pattern.get(), FC_FAMILY,
                            reinterpret_cast<const FcChar8*>(family.c_str()))) {
        return none;
    }
    return {FcFontList(nullptr, pattern.get(), objects.get()),
            &FcFontSetDestroy};
}

bool same_style(const std::string& a, const std::string& b)
{
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) ==
                      std::tolower(static_cast<unsigned char>(y));
           });
}

} // namespace

std::string font_name_text(int platform, const std::string& bytes)
{
    std::string text;
    if (platform == 3 || platform == 0) { // UTF-16BE
        for (size_t i = 0; i + 1 < bytes.size(); i += 2) {
            unsigned c = unit(bytes, i);
            if (c >= 0xd800 && c < 0xdc00 && i + 3 < bytes.size()) {
                const unsigned low = unit(bytes, i + 2);
                if (low >= 0xdc00 && low < 0xe000) {
                    c = 0x10000 + ((c - 0xd800) << 10) + (low - 0xdc00);
                    i += 2;
                }
            }
            if (c >= 0xd800 && c < 0xe000) { // a lone surrogate
                c = 0xfffd;
            }
            append_utf8(text, c);
        }
    } else if (platform == 1) {
        for (const char c : bytes) {
            text += static_cast<unsigned char>(c) < 0x80 ? c : '?';
        }
    }
    return text;
}

std::vector<InfoRow> font_file_info(const std::string& file, int face_index)
{
    // the order of the rows is the order of the name ids of the table
    struct Wanted {
        FT_UShort id = 0;
        const char* label = "";
    };
    static const Wanted wanted[] = {
            {4, "Full name"},    {5, "Version"},      {6, "PostScript name"},
            {9, "Designer"},     {8, "Manufacturer"}, {12, "Designer URL"},
            {11, "Vendor URL"},  {10, "Description"}, {13, "License"},
            {14, "License URL"}, {7, "Trademark"},    {0, "Copyright"}};
    std::vector<InfoRow> rows;
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library) != 0) {
        return rows;
    }
    FT_Face face = nullptr;
    if (FT_New_Face(library, file.c_str(), face_index, &face) == 0) {
        const FT_UInt count = FT_Get_Sfnt_Name_Count(face);
        for (const auto& entry : wanted) {
            std::string text;
            for (FT_UInt i = 0; i < count && text.empty(); ++i) {
                FT_SfntName name;
                if (FT_Get_Sfnt_Name(face, i, &name) != 0 ||
                    name.name_id != entry.id || !is_english(name)) {
                    continue;
                }
                text = font_name_text(
                        name.platform_id,
                        std::string(reinterpret_cast<const char*>(name.string),
                                    name.string_len));
            }
            if (!text.empty()) {
                rows.push_back({entry.label, text, false});
            }
        }
        FT_Done_Face(face);
    }
    FT_Done_FreeType(library);
    return rows;
}

std::vector<InfoRow> font_info(const std::string& family)
{
    std::vector<InfoRow> rows;
    const FaceSet faces = faces_of(family);
    if (!faces || faces->nfont == 0) {
        return rows;
    }

    // the main face is the plain one, as in the list of the styles
    const std::vector<std::string> styles = font_styles(family);
    const FcPattern* main = faces->fonts[0];
    for (int i = 0; i < faces->nfont && !styles.empty(); ++i) {
        if (same_style(string_of(faces->fonts[i], FC_STYLE), styles.front())) {
            main = faces->fonts[i];
            break;
        }
    }

    rows.push_back({"Family", strings_of(main, FC_FAMILY), false});
    std::string style_list;
    for (const std::string& style : styles) {
        style_list += (style_list.empty() ? "" : ", ") + style;
    }
    rows.push_back({"Styles", style_list, false});
    const std::string format = string_of(main, FC_FONTFORMAT);
    if (!format.empty()) {
        rows.push_back({"Format", format, false});
    }
    int spacing = FC_PROPORTIONAL;
    FcPatternGetInteger(main, FC_SPACING, 0, &spacing);
    rows.push_back({"Spacing", spacing_name(spacing), false});
    FcBool flag = FcFalse;
    FcPatternGetBool(main, FC_VARIABLE, 0, &flag);
    rows.push_back({"Variable", flag ? "Yes" : "No", false});
    flag = FcFalse;
    FcPatternGetBool(main, FC_COLOR, 0, &flag);
    rows.push_back({"Color", flag ? "Yes" : "No", false});
    const std::string foundry = string_of(main, FC_FOUNDRY);
    if (!foundry.empty() && foundry != "unknown") {
        rows.push_back({"Foundry", foundry, false});
    }
    FcCharSet* chars = nullptr;
    if (FcPatternGetCharSet(main, FC_CHARSET, 0, &chars) == FcResultMatch &&
        chars != nullptr) {
        rows.push_back(
                {"Characters",
                 number_with_commas(static_cast<int>(FcCharSetCount(chars))),
                 false});
    }
    const std::string langs = languages_of(main);
    if (!langs.empty()) {
        rows.push_back({"Languages", langs, false});
    }

    // what the file says about itself
    int index = 0;
    FcPatternGetInteger(main, FC_INDEX, 0, &index);
    const std::vector<InfoRow> own =
            font_file_info(string_of(main, FC_FILE), index & 0xffff);
    if (!own.empty()) {
        rows.push_back({"About the font", "", true});
        rows.insert(rows.end(), own.begin(), own.end());
    }

    // the files of all the faces
    rows.push_back({"Files", "", true});
    for (int i = 0; i < faces->nfont; ++i) {
        const FcPattern* face = faces->fonts[i];
        const std::string style = string_of(face, FC_STYLE);
        rows.push_back({style.empty() ? "Regular" : style,
                        string_of(face, FC_FILE), false});
    }
    std::stable_sort(rows.end() - faces->nfont, rows.end(),
                     [](const InfoRow& a, const InfoRow& b) {
                         return a.label < b.label;
                     });
    return rows;
}

} // namespace tiny::xdg
