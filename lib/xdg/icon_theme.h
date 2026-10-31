#pragma once

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tiny::xdg {

/**
 * @short The icon directories of the user, then of the system.
 *
 * `$HOME/.icons`, then `icons` in every XDG data directory (see
 * `data_dir_list`). The arguments are the values of the variables.
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
 * `hicolor` at the end, then the files right in the icon directories (GTK
 * does that too) and the pixmap directories. In a theme a bitmap (`.png`,
 * `.xpm`) of the wanted size wins, then an `.svg` (it can be drawn in any
 * size), then the bitmap with the closest size. When no directory has an
 * index.theme of `hicolor` (icons installed by hand, a system that keeps
 * the themes elsewhere) its `NxN/apps` and `scalable/apps` directories are
 * used. Results are cached, and so are the listings of the directories (an
 * icon installed later is not seen), so one object should live as long as
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

    //! Is there such a file in the directory? Directories are listed once.
    bool exists(const std::string& dir, const std::string& file) const;
    std::string find_uncached(std::string_view name, int size) const;
    std::string find_in(const Theme& theme, std::string_view name,
                        int size) const;
    void load(const std::string& name, std::vector<std::string>& parents);
    //! hicolor with no index.theme: the directories are guessed from names
    void guess_hicolor();

    std::vector<std::string> bases;
    std::vector<std::string> pixmaps;
    std::vector<Theme> themes; //!< the theme, its parents, hicolor
    mutable std::map<std::pair<std::string, int>, std::string> cache;
    mutable std::map<std::string, std::set<std::string>> listings;
};

} // namespace tiny::xdg
