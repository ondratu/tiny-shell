#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "style.h"
#include "wm/dock_mode.h"
#include "wm/monitors.h"

namespace tiny::wm {

/**
 * @short The settings of tiny-shell, from the configuration files.
 *
 * The members have the defaults that are used when no file sets them.
 *
 * ```
 * [theme]
 * scale = 1.5
 * font = DejaVu Sans,sans
 * font_size = 12
 * font_style = Bold
 * style = dark
 * xsettings = yes
 *
 * [xft]
 * antialias = yes
 * hinting = slight
 * rgba = rgb
 *
 * [wallpaper]
 * file = ~/Pictures/mountains.jpg
 * mode = fill
 *
 * [commands]
 * reboot = sudo reboot
 * poweroff = doas poweroff
 * terminal = xterm -e
 * lock = tiny-lock
 *
 * [lock]
 * auth = auto
 * pam_service = tiny-lock
 * delay = 5
 *
 * [display]
 * off_after = 15
 *
 * [panel]
 * clock_format = %a %e %B %R
 * system_tray = yes
 *
 * [monitors]
 * primary = auto
 * secondary = 2
 * placement = right
 *
 * [dock]
 * mode = autohide
 * favorites = firefox.desktop;org.gnome.Nautilus.desktop
 * ```
 */
struct Config {
    double scale = 1;                                //!< [theme] scale, (0, 16]
    std::string reboot_command = "systemctl reboot"; //!< [commands]
    std::string poweroff_command = "systemctl poweroff"; //!< [commands]
    std::string lock_command = "tiny-lock";              //!< [commands] lock
    std::string lock_auth =
            "auto"; //!< [lock] auth: auto, pam, checker, pwcheck
    std::string lock_pam_service = "tiny-lock"; //!< [lock] pam_service
    //! [lock] delay: minutes from the moment the display is switched off to
    //! the lock (0, 1, 2, 5, 10 or 30); 0 is at once, -1 is `never`, the
    //! default
    int lock_delay_minutes = -1;
    //! [display] off_after: minutes without input to switch the display off
    //! (5, 10, 15, 30 or 60); 0 is `never` (the power saving is switched off),
    //! -1 leaves the X server as it is, the default
    int display_off_minutes = -1;
    std::string
            lock_checker; //!< [lock] checker: path of unix_chkpwd, "" is searched
    std::string
            lock_pwcheck; //!< [lock] pwcheck: path of tiny-lock-pwcheck, "" is the one next to the lock
    std::vector<std::string> terminal = {"xterm", "-e"}; //!< [commands]
    std::string terminal_line = "xterm -e"; //!< the text of `terminal`
    std::string font =
            "DejaVu Sans,sans";    //!< [theme] font, families of fontconfig
    double font_size = 12;         //!< [theme] font_size, points at scale 1
    std::string font_style;        //!< [theme] font_style, "" is the default
    std::string xft_antialias;     //!< [xft] antialias, "" is not set
    std::string xft_hinting;       //!< [xft] hinting, "" is not set
    std::string xft_rgba;          //!< [xft] rgba, "" is not set
    Variant style = Variant::Dark; //!< [theme] style, dark or light
    //! [theme] xsettings: tell GTK and Qt the look (yes or no)
    bool xsettings = true;
    //! [wallpaper] file, a PNG or JPEG; "" is no wallpaper (`~/` is the home)
    std::string wallpaper_file;
    //! [wallpaper] file is in a file (empty or not). When it is not, the
    //! wallpaper is the default one of the style: see `wallpaper_of()`.
    bool wallpaper_set = false;
    //! [wallpaper] mode, one of `wallpaper_choices()`; "" is fill
    std::string wallpaper_mode;
    //! [panel] clock_format, a `strftime()` format of the clock; "" is no clock
    std::string clock_format = "%a %e %B %R";
    //! [panel] system_tray: the icons of programs in the panel (yes or no)
    bool system_tray = true;
    //! [monitors] primary: the monitor of the panel and the dock, a number
    //! (as the monitors are numbered), a name of an output or "auto"
    std::string monitor_primary = "auto";
    //! [monitors] secondary: the monitor that `monitor_placement` moves
    std::string monitor_secondary = "auto";
    //! [monitors] placement: where the secondary monitor is, seen from the
    //! primary one; `Keep` leaves the layout of the system alone
    Placement monitor_placement = Placement::Keep;
    //! [dock] mode: autohide (the default), on or off
    DockMode dock_mode = DockMode::AutoHide;
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
 * @short The values that `[dock] mode` can have: "" (autohide, the default),
 * "on" and "off", with the names a form shows.
 */
XftChoices dock_choices();

//! Whether the text is one of the values of `dock_choices()`
bool valid_dock_choice(const std::string& text);

/**
 * @short The values that `[monitors] placement` can have: "" (keep), right,
 * left, above and below, with the names a form shows.
 */
XftChoices placement_choices();

//! Whether the text is one of the values of `placement_choices()`
bool valid_placement_choice(const std::string& text);

/**
 * @short Whether the text is a format of the clock that can be saved.
 *
 * Empty (no clock) or a `strftime()` format without a control character (a
 * line break would break the file).
 */
bool valid_clock_format(const std::string& text);

//! The choices of `[lock] delay` that a form offers: "" is `never` (the
//! default), then at once (0), 1, 2, 5, 10 and 30 minutes
XftChoices lock_delay_choices();

//! Whether the text is one of the values of `lock_delay_choices()`, or "never"
//! (the same as "")
bool valid_lock_delay_choice(const std::string& text);

//! The values of `[lock] delay` in minutes; -1 is never
bool parse_lock_delay(const std::string& text, int& minutes);

//! The choices of `[display] off_after` that a form offers: "" leaves the
//! server as it is (the default), then `never`, 5, 10, 15, 30 and 60 minutes
XftChoices display_off_choices();

//! Whether the text is one of the values of `display_off_choices()`
bool valid_display_off_choice(const std::string& text);

//! The values of `[display] off_after` in minutes; 0 is never, -1 (an empty
//! text) leaves the X server as it is
bool parse_display_off(const std::string& text, int& minutes);

//! A name of a PAM service: no blanks, no slashes, no control characters
bool valid_pam_service(const std::string& text);

//! A path of a program: not empty, no control characters
bool valid_program_path(const std::string& text);

/**
 * @short The ways a wallpaper covers the screen: fill, center and tile.
 *
 * The first one is "" (the default, fill). The type is the one of the options
 * of `[xft]`, a value and the name that a form shows.
 */
XftChoices wallpaper_choices();

//! Whether the text is one of `wallpaper_choices()` or "fill" (the same as "")
bool valid_wallpaper_mode(const std::string& text);

/**
 * @short The wallpaper that the shell installs for a style: the fox
 * by day for the light one, the fox by night for the dark one.
 *
 * It is `backgrounds/tiny-shell/tiny-shell-fox-<day|night>-1920x1080.jpg` in
 * the first of the data directories that has it; empty when none has (it is not
 * installed).
 */
std::string default_wallpaper(Variant variant,
                              const std::vector<std::string>& data_dirs);

/**
 * @short The wallpaper to draw: `[wallpaper] file` when the configuration has
 * the key (an empty one is no wallpaper), else `default_wallpaper()` of the
 * style.
 */
std::string wallpaper_of(const Config& config, Variant variant,
                         const std::vector<std::string>& data_dirs);

/**
 * @short Whether the text is a file of a wallpaper that can be saved.
 *
 * Empty (no wallpaper) or an absolute path (or `~/...`) without a control
 * character, because the program that draws it starts in another directory.
 * Whether the
 * file exists is not checked: it can be on a disk that is not mounted yet.
 */
bool valid_wallpaper_file(const std::string& text);

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
