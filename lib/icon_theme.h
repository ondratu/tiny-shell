#pragma once

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tiny {

/**
 * @short The icon directories of the user, then of the system.
 *
 * `$HOME/.icons`, then `icons` in every XDG data directory (see
 * `xdg_data_dirs`). The arguments are the values of the variables.
 */
std::vector<std::string> icon_dirs(const char* data_home, const char* data_dirs,
                                   const char* home);

/// The `pixmaps` directory of every XDG data directory.
std::vector<std::string> pixmap_dirs(const char* data_home,
                                     const char* data_dirs, const char* home);

/**
 * @short Find the file of an icon by its name (the Icon key of a desktop
 * entry).
 *
 * Follows the Icon Theme Specification: the theme and the themes it inherits,
 * `hicolor` at the end, then the pixmap directories. In a theme a bitmap
 * (`.png`, `.xpm`) of the wanted size wins, then an `.svg` (it can be drawn
 * in any size), then the bitmap with the closest size. Results are cached, so one object should live as long as
 * the icons are needed; it is not thread safe.
 */
class IconTheme {
  public:
    /// `bases` are the directories that hold the themes, `pixmaps` the
    /// last resort. The themes are read here.
    IconTheme(std::vector<std::string> bases, std::vector<std::string> pixmaps,
              std::string_view theme = "hicolor");

    /// The path of the file, empty when there is none. An absolute name is
    /// returned as it is when the file exists.
    std::string find(std::string_view name, int size) const;

  private:
    struct SubDir {
        std::string name;
        int size = 0;
        int min_size = 0;
        int max_size = 0;
        int threshold = 2;
        enum Type { FIXED, SCALABLE, THRESHOLD } type = THRESHOLD;

        int distance(int wanted) const;
    };

    struct Theme {
        std::string name;
        std::vector<SubDir> dirs;
    };

    std::string find_uncached(std::string_view name, int size) const;
    std::string find_in(const Theme& theme, std::string_view name,
                        int size) const;
    void load(const std::string& name, std::vector<std::string>& parents);

    std::vector<std::string> bases;
    std::vector<std::string> pixmaps;
    std::vector<Theme> themes; //!< the theme, its parents, hicolor
    mutable std::map<std::pair<std::string, int>, std::string> cache;
};

} // namespace tiny
