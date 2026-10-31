#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "xdg/desktop_entry.h"

namespace tiny::xdg {

/// One installed application found in an `applications` directory.
struct Application {
    std::string id;   //!< desktop file id, e.g. "kde-okular.desktop"
    std::string path; //!< the file the entry was read from
    DesktopEntry entry;
};

/**
 * @short The `applications` directories in the order of priority.
 *
 * `$XDG_DATA_HOME` (or `$HOME/.local/share`) first, then `$XDG_DATA_DIRS`
 * (or `/usr/local/share:/usr/share`). Pass the values of the variables, null
 * or empty means unset. Relative paths are ignored (as the specification
 * says) and a directory is listed only once.
 */
std::vector<std::string> application_dirs(const char* data_home,
                                          const char* data_dirs,
                                          const char* home);

/**
 * @short Does `program` exist as an executable file?
 *
 * A name with a slash is a path (relative to the working directory), other
 * names are looked up in the directories of `path`.
 */
bool executable_exists(std::string_view program, std::string_view path);

/**
 * @short The applications the user can start, sorted by name.
 *
 * A file in an earlier directory hides the file with the same id in a later
 * one (also when it is Hidden), a file that can't be read hides nothing.
 * Entries with a missing TryExec binary are dropped. The names are sorted by
 * `strcoll`, so call `setlocale(LC_COLLATE, "")` for the order of the user.
 */
class ApplicationIndex {
  public:
    /// Read the directories (the earlier wins) and replace the content.
    /// TryExec is looked up in `$PATH`.
    void scan(const std::vector<std::string>& dirs,
              std::string_view locale = "");

    /// The same with the directories of TryExec given in `path`.
    void scan(const std::vector<std::string>& dirs, std::string_view locale,
              std::string_view path);

    const std::vector<Application>& applications() const { return apps; }

  private:
    std::vector<Application> apps;
};

} // namespace tiny::xdg
