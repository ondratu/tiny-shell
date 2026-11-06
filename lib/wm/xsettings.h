#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "style.h"
#include "wm/config.h"
#include "wm/selection.h"

namespace tiny::wm {

//! One setting of XSETTINGS: a name with a number or a text
struct XSetting {
    enum class Type : uint8_t { Integer = 0, String = 1 };

    static XSetting integer(std::string name, int value)
    {
        return {Type::Integer, std::move(name), value, {}, 0};
    }

    static XSetting string(std::string name, std::string value)
    {
        return {Type::String, std::move(name), 0, std::move(value), 0};
    }

    bool same_value(const XSetting& other) const
    {
        return type == other.type && name == other.name &&
               number == other.number && text == other.text;
    }

    Type type = Type::Integer;
    std::string name;     //!< like `Net/ThemeName`
    int number = 0;       //!< the value of an integer
    std::string text;     //!< the value of a string
    uint32_t changed = 0; //!< the serial of the change of the value
};

/**
 * @short The value of the property `_XSETTINGS_SETTINGS` (XSETTINGS 0.5).
 *
 * Bytes of the header (the byte order, the serial and the number of the
 * settings) and of every setting, in the byte order of this machine.
 */
std::vector<unsigned char>
encode_xsettings(const std::vector<XSetting>& settings, uint32_t serial);

/**
 * @short What the programs (GTK, Qt) are told about the look.
 *
 * - `Net/ThemeName`: Adwaita-dark for the dark variant, Adwaita for the light
 *   one (GTK 2 and 3 know no other way to choose a dark theme).
 * - `Gtk/FontName`: the first family of `[theme] font`, the style and the
 *   size, like Pango writes it ("DejaVu Sans Bold 12").
 * - `Xft/DPI`, `Gdk/UnscaledDPI` and `Gdk/WindowScalingFactor`: from
 *   `[theme] scale`, the way a GNOME settings daemon writes them (96 dpi at the
 *   scale 1; the whole part of the scale is the factor of the windows, the
 *   rest is the DPI).
 * - `Xft/Antialias`, `Xft/Hinting`, `Xft/HintStyle` and `Xft/RGBA`: what
 *   `[xft]` sets; what it does not set is left out.
 *
 * Sorted by name.
 */
std::vector<XSetting> xsettings_of(const Config& config, Variant variant);

/**
 * @short The owner of `_XSETTINGS_Sn`, the manager of settings.
 *
 * The selection is owned by a window of ours. The programs read the settings
 * from its property `_XSETTINGS_SETTINGS` and follow its changes, so a change
 * of the variant is seen by the programs that run at once.
 */
class XSettings {
  public:
    XSettings(::Display* display, ::Window root, int screen);

    ~XSettings();

    XSettings(const XSettings&) = delete;
    XSettings& operator=(const XSettings&) = delete;

    //! Become the manager of the settings and tell the programs; false when
    //! another one (a settings daemon) is there
    bool acquire();

    //! Publish these settings; nothing changes when they are the same as the
    //! ones that are there. Before `acquire()` they are kept for it.
    void set(std::vector<XSetting> list);

    //! The serial of the last change (the one in the property)
    uint32_t get_serial() const { return serial; }

    ::Window get_window() const { return selection.get_window(); }

    //! The selection of the `SelectionClear` event is the one we hold
    bool is_lost_by(const XSelectionClearEvent& e) const
    {
        return selection.is_lost_by(e);
    }

  private:
    void publish();

    ::Display* display;
    Selection selection;
    std::vector<XSetting> settings;
    uint32_t serial = 0;
    bool acquired = false;
};

} // namespace tiny::wm
