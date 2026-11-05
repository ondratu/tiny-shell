#pragma once

#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace tiny::wm {

/**
 * @short The symbolic icons of the shell (the logo, the buttons of a window,
 * the generic application): one color shapes that take the color of the text.
 *
 * The files are `tiny-shell/icons/white/<name>-symbolic.svg` in the XDG data
 * directories (`make install` puts them there). They are drawn in any size
 * and painted with the color that is asked for, so a variant of the theme
 * needs no other files. Without the file nothing is drawn and `draw()` says
 * so: the caller draws what it drew before. An icon is made once for a name,
 * a size and a color and kept until the object dies.
 */
class SymbolicIcons {
  public:
    //! A made icon; the object owns the X resources
    struct Icon {
        Pixmap pixmap = None;
        Picture picture = None;
        int width = 0;
        int height = 0;
    };

    //! `data_dirs` are the directories that have `tiny-shell/icons`
    SymbolicIcons(::Display* display, ::Window root,
                  std::vector<std::string> data_dirs);

    SymbolicIcons(const SymbolicIcons&) = delete;
    SymbolicIcons& operator=(const SymbolicIcons&) = delete;
    ~SymbolicIcons();

    //! The directories of the user and the system, `/usr/local/share` too
    static std::vector<std::string> default_dirs();

    //! The path of the file of an icon, empty when there is none
    std::string find(std::string_view name) const;

    //! The icon `size` pixels high or wide (the longer side), in the color
    //! 0xRRGGBB; `picture` is None when there is no such icon
    const Icon& get(const std::string& name, int size, unsigned long color);

    /**
     * @short Draw the icon in the middle of the box of the window.
     *
     * The icon is put over what the window shows. Returns false and draws
     * nothing when the icon does not exist.
     */
    bool draw(::Window target, const std::string& name, int size,
              unsigned long color, int box_width, int box_height);

  private:
    ::Display* display;
    ::Window root;
    std::vector<std::string> dirs;
    std::map<std::tuple<std::string, int, unsigned long>, Icon> icons;
};

} // namespace tiny::wm
