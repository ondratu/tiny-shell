#include "wm/xsettings.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace tiny::wm {

namespace {

//! The name of the selection of the manager of settings
std::string selection_of(int screen)
{
    return "_XSETTINGS_S" + std::to_string(screen);
}

void put32(std::vector<unsigned char>& out, uint32_t value)
{
    unsigned char bytes[4];
    std::memcpy(bytes, &value, 4); // the byte order of the header is ours
    out.insert(out.end(), bytes, bytes + 4);
}

void put16(std::vector<unsigned char>& out, uint16_t value)
{
    unsigned char bytes[2];
    std::memcpy(bytes, &value, 2);
    out.insert(out.end(), bytes, bytes + 2);
}

void pad(std::vector<unsigned char>& out)
{
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

bool is_big_endian()
{
    const uint16_t one = 1;
    unsigned char first = 0;
    std::memcpy(&first, &one, 1);
    return first == 0;
}

//! "DejaVu Sans,sans" is the list of families of fontconfig: the first one
std::string first_family(const std::string& families)
{
    return families.substr(0, families.find(','));
}

//! Up to 3 decimals, without trailing zeros: the size of a font in Pango
std::string size_text(double size)
{
    char text[32];
    snprintf(text, sizeof(text), "%.3f", size);
    std::string rv = text;
    while (rv.back() == '0') {
        rv.pop_back();
    }
    if (rv.back() == '.') {
        rv.pop_back();
    }
    return rv;
}

int dpi_of(double scale)
{
    return static_cast<int>(std::lround(96.0 * 1024.0 * scale));
}

} // namespace

std::vector<unsigned char>
encode_xsettings(const std::vector<XSetting>& settings, uint32_t serial)
{
    std::vector<unsigned char> out;
    out.push_back(is_big_endian() ? 1 : 0); // MSBFirst is 1, LSBFirst is 0
    out.insert(out.end(), 3, 0);
    put32(out, serial);
    put32(out, static_cast<uint32_t>(settings.size()));
    for (const XSetting& setting : settings) {
        out.push_back(static_cast<unsigned char>(setting.type));
        out.push_back(0);
        put16(out, static_cast<uint16_t>(setting.name.size()));
        out.insert(out.end(), setting.name.begin(), setting.name.end());
        pad(out);
        put32(out, setting.changed);
        if (setting.type == XSetting::Type::Integer) {
            put32(out, static_cast<uint32_t>(setting.number));
        } else {
            put32(out, static_cast<uint32_t>(setting.text.size()));
            out.insert(out.end(), setting.text.begin(), setting.text.end());
            pad(out);
        }
    }
    return out;
}

std::vector<XSetting> xsettings_of(const Config& config, Variant variant)
{
    std::vector<XSetting> rv;
    rv.push_back(XSetting::string("Net/ThemeName", variant == Variant::Dark
                                                           ? "Adwaita-dark"
                                                           : "Adwaita"));

    std::string font = first_family(config.font);
    if (!config.font_style.empty()) {
        font += ' ' + config.font_style;
    }
    rv.push_back(XSetting::string("Gtk/FontName",
                                  font + ' ' + size_text(config.font_size)));

    // the windows are scaled by a whole number and the text by the rest
    const double whole = std::max(1.0, std::floor(config.scale));
    rv.push_back(XSetting::integer("Gdk/WindowScalingFactor",
                                   static_cast<int>(whole)));
    rv.push_back(
            XSetting::integer("Gdk/UnscaledDPI", dpi_of(config.scale / whole)));
    rv.push_back(XSetting::integer("Xft/DPI", dpi_of(config.scale)));

    if (!config.xft_antialias.empty()) {
        rv.push_back(XSetting::integer("Xft/Antialias",
                                       config.xft_antialias == "yes" ? 1 : 0));
    }
    if (!config.xft_hinting.empty()) {
        rv.push_back(XSetting::integer("Xft/Hinting",
                                       config.xft_hinting == "none" ? 0 : 1));
        rv.push_back(
                XSetting::string("Xft/HintStyle", "hint" + config.xft_hinting));
    }
    if (!config.xft_rgba.empty()) {
        rv.push_back(XSetting::string("Xft/RGBA", config.xft_rgba));
    }
    std::sort(rv.begin(), rv.end(), [](const XSetting& a, const XSetting& b) {
        return a.name < b.name;
    });
    return rv;
}

XSettings::XSettings(::Display* display_arg, ::Window root, int screen):
    display(display_arg), selection(display_arg, root, selection_of(screen))
{}

XSettings::~XSettings() = default;

bool XSettings::acquire()
{
    if (selection.acquire(false, std::chrono::milliseconds(0)) !=
        Selection::Result::Acquired) {
        return false;
    }
    acquired = true;
    serial = 0;
    for (XSetting& setting : settings) {
        setting.changed = serial;
    }
    publish(); // before the announcement: the programs read it at once
    selection.announce();
    return true;
}

void XSettings::set(std::vector<XSetting> list)
{
    bool changed = list.size() != settings.size();
    for (size_t i = 0; !changed && i < list.size(); ++i) {
        changed = !list[i].same_value(settings[i]);
    }
    if (!changed) {
        return;
    }
    ++serial;
    for (XSetting& setting : list) {
        // the serial of a change of a value: the new one when it is not the
        // same as before
        const auto old = std::find_if(
                settings.begin(), settings.end(),
                [&](const XSetting& s) { return s.name == setting.name; });
        setting.changed = old != settings.end() && old->same_value(setting)
                                  ? old->changed
                                  : serial;
    }
    settings = std::move(list);
    if (acquired) {
        publish();
    }
}

void XSettings::publish()
{
    const std::vector<unsigned char> value = encode_xsettings(settings, serial);
    const Atom property = XInternAtom(display, "_XSETTINGS_SETTINGS", False);
    XChangeProperty(display, selection.get_window(), property, property, 8,
                    PropModeReplace, value.data(),
                    static_cast<int>(value.size()));
    XFlush(display);
}

} // namespace tiny::wm
