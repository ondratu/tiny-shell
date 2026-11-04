#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "style.h"

namespace tiny::wm {

/**
 * @short The settings of tiny-shell, from the configuration files.
 *
 * The members have the defaults that are used when no file sets them.
 *
 * ```
 * [theme]
 * scale = 1.5
 * font = Cantarell
 * font_size = 12
 * font_style = Bold
 * style = dark
 *
 * [xft]
 * antialias = yes
 * hinting = slight
 * rgba = rgb
 *
 * [commands]
 * reboot = sudo reboot
 * poweroff = doas poweroff
 * terminal = xterm -e
 *
 * [dock]
 * favorites = firefox.desktop;org.gnome.Nautilus.desktop
 * ```
 */
struct Config {
    double scale = 1;                                //!< [theme] scale, (0, 16]
    std::string reboot_command = "systemctl reboot"; //!< [commands]
    std::string poweroff_command = "systemctl poweroff"; //!< [commands]
    std::vector<std::string> terminal = {"xterm", "-e"}; //!< [commands]
    std::string terminal_line = "xterm -e"; //!< the text of `terminal`
    std::string font = "Cantarell"; //!< [theme] font, a family of fontconfig
    double font_size = 12;          //!< [theme] font_size, points at scale 1
    std::string font_style;         //!< [theme] font_style, "" is the default
    std::string xft_antialias;      //!< [xft] antialias, "" is not set
    std::string xft_hinting;        //!< [xft] hinting, "" is not set
    std::string xft_rgba;           //!< [xft] rgba, "" is not set
    Variant style = Variant::Dark;  //!< [theme] style, dark or light
    //! [dock] favorites: ids of desktop files, `;` between them
    std::vector<std::string> favorites;
};

//! The ways the programs draw text, that `[xft]` sets (`Xft.*` of X resources)
enum class XftOption { Antialias, Hinting, Rgba };

//! A value of `[xft]` and the name that a form shows for it
struct XftChoice {
    const char* value;
    const char* label;
};

//! The values of an option, to loop over
struct XftChoices {
    const XftChoice* first;
    const XftChoice* last;

    const XftChoice* begin() const { return first; }
    const XftChoice* end() const { return last; }
    size_t size() const { return static_cast<size_t>(last - first); }
    const XftChoice& operator[](size_t i) const { return first[i]; }
};

/**
 * @short The values that an option of `[xft]` can have.
 *
 * The first one is always "" (not set: the programs keep what the system
 * says), shown as "Default".
 */
XftChoices xft_choices(XftOption option);

//! The key of the option in `[xft]`
const char* xft_key(XftOption option);

//! Whether the text is one of `xft_choices(option)`
bool valid_xft(XftOption option, const std::string& text);

/**
 * @short The scale from the text of a setting.
 *
 * Nothing when the text (blanks at both ends are ignored) is not a number
 * above 0 and up to 16.
 */
std::optional<double> parse_scale(const std::string& text);

//! The limits of the size of the font, in points
constexpr double min_font_size = 4;
constexpr double max_font_size = 96;

/**
 * @short The size of the font from the text of a setting.
 *
 * Nothing when the text (blanks at both ends are ignored) is not a number from
 * `min_font_size` to `max_font_size`.
 */
std::optional<double> parse_font_size(const std::string& text);

/**
 * @short Whether the text is a style of a font that can be saved.
 *
 * Empty (the default style) or a name without a control character, like
 * "Bold Italic".
 */
bool valid_font_style(const std::string& text);

/**
 * @short Whether the text is a name of a font that can be saved.
 *
 * It is not empty (blanks at both ends are ignored) and has no control
 * character. Whether the family is installed is not checked: fontconfig
 * gives a font that is close when it is not.
 */
bool valid_font(const std::string& text);

/**
 * @short The files to read, the one with the lowest priority first.
 *
 * `tiny-shell.ini` in every directory of the XDG configuration directories
 * (`$XDG_CONFIG_DIRS`, `/etc/xdg` by default), then the one of the user
 * (`$XDG_CONFIG_HOME`, `~/.config` by default). Pass the values of the
 * variables, null or empty means unset.
 */
std::vector<std::string> config_files(const char* config_home,
                                      const char* config_dirs,
                                      const char* home);

/**
 * @short The file of the user, the one that tiny-shell writes.
 *
 * `tiny-shell.ini` in `$XDG_CONFIG_HOME` (or `$HOME/.config`), empty when
 * neither is known. It is the last of `config_files`.
 */
std::string user_config_file(const char* config_home, const char* home);

//! The text of a configuration file and its name for the messages
struct ConfigSource {
    std::string name;
    std::string text;
};

/**
 * @short Settings from the texts of the files, layered in the order given
 * (a later one wins).
 *
 * A line or a value that is wrong is described in `problems` (a line with the
 * name of its source) and the default is kept, or the value of a source with
 * a lower priority. Keys that are not known are ignored, so a file can be
 * shared with a newer version.
 */
Config parse_config(const std::vector<ConfigSource>& sources,
                    std::vector<std::string>* problems = nullptr);

/// Read the files (a file that is missing is not an error), see `parse_config`.
Config load_config(const std::vector<std::string>& files,
                   std::vector<std::string>* problems = nullptr);

//! A value of the ini file to write
struct IniValue {
    std::string section;
    std::string key;
    std::string value;
};

/**
 * @short Write values to the file of the user.
 *
 * The file is made again from the values it has (a comment is lost, a line
 * that was wrong is gone), with the given ones set. It is written to a
 * temporary file first and renamed, so a failure leaves the old file. A name
 * or a value that would break the file (a line break, an empty key, `=` in a
 * key, `]` in a section) is refused and nothing is written. Returns false and
 * tells why in `error`, when it is given.
 */
bool save_values(const std::string& file, const std::vector<IniValue>& values,
                 std::string* error = nullptr);

/**
 * @short Write the favorite applications to the file of the user.
 *
 * Sets `[dock] favorites` the way `save_values` does.
 */
bool save_favorites(const std::string& file,
                    const std::vector<std::string>& favorites,
                    std::string* error = nullptr);

} // namespace tiny::wm
