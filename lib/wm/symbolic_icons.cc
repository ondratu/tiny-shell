#include "wm/symbolic_icons.h"

#include <filesystem>

#include <cstdlib>

#include "image/image.h"
#include "image/pixmap.h"
#include "x_util.h"
#include "xdg/application_index.h"
#include "xdg/dirs.h"

namespace tiny::wm {

SymbolicIcons::SymbolicIcons(::Display* display_arg, ::Window root_arg,
                             std::vector<std::string> data_dirs):
    display(display_arg), root(root_arg), dirs(std::move(data_dirs))
{}

SymbolicIcons::~SymbolicIcons()
{
    for (auto& [key, icon] : icons) {
        if (icon.picture != None) {
            XRenderFreePicture(display, icon.picture);
        }
        if (icon.pixmap != None) {
            XFreePixmap(display, icon.pixmap);
        }
    }
}

std::vector<std::string> SymbolicIcons::default_dirs()
{
    const std::string with_local =
            xdg::data_dirs_with(getenv("XDG_DATA_DIRS"), xdg::local_share_dir);
    return xdg::data_dir_list(getenv("XDG_DATA_HOME"),
                              with_local.empty() ? nullptr : with_local.c_str(),
                              getenv("HOME"));
}

std::string SymbolicIcons::find(std::string_view name) const
{
    for (const std::string& dir : dirs) {
        const std::string file = dir + "/tiny-shell/icons/white/" +
                                 std::string(name) + "-symbolic.svg";
        if (std::filesystem::is_regular_file(file)) {
            return file;
        }
    }
    return {};
}

const SymbolicIcons::Icon& SymbolicIcons::get(const std::string& name, int size,
                                              unsigned long color)
{
    const auto key = std::make_tuple(name, size, color);
    const auto found = icons.find(key);
    if (found != icons.end()) {
        return found->second;
    }
    Icon icon; // also the one that is not there, so the disk is read once
    const std::string file = find(name);
    if (!file.empty()) {
        const auto loaded = image::load_image(file, size);
        if (loaded) {
            const image::Image painted =
                    image::tint(*loaded, static_cast<uint32_t>(color));
            icon.pixmap = image::create_pixmap(display, root, painted);
            if (icon.pixmap != None) {
                icon.picture = image::create_argb_picture(display, icon.pixmap);
                icon.width = painted.width;
                icon.height = painted.height;
            }
        }
    }
    return icons.emplace(key, icon).first->second;
}

bool SymbolicIcons::draw(::Window target, const std::string& name, int size,
                         unsigned long color, int box_width, int box_height)
{
    const Icon& icon = get(name, size, color);
    if (icon.picture == None) {
        return false;
    }
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, target, &attrs) == 0) {
        return false;
    }
    const XRenderPictFormat* format =
            XRenderFindVisualFormat(display, attrs.visual);
    if (format == nullptr) {
        return false;
    }
    const Picture destination =
            XRenderCreatePicture(display, target, format, 0, nullptr);
    XRenderComposite(display, PictOpOver, icon.picture, None, destination, 0, 0,
                     0, 0, (box_width - icon.width) / 2,
                     (box_height - icon.height) / 2, x_uint(icon.width),
                     x_uint(icon.height));
    XRenderFreePicture(display, destination);
    return true;
}

} // namespace tiny::wm
