#include <X11/keysym.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "image/image.h"
#include "image/pixmap.h"
#include "wm/launcher.h"
#include "x_util.h"
#include "xdg/application_launch.h"
#include "xdg/application_search.h"

namespace tiny::wm {

namespace {

constexpr size_t recent_count = 16;

int spacing()
{
    return tiny::theme.wm_win_border;
}

// The border of the entry is outside of its size
int entry_height()
{
    return tiny::theme.wm_win_header;
}

// The grid is smaller than the theme says when the screen is small
int screen_size(bool horizontal)
{
    const int screen = XDefaultScreen(tiny::get_display());
    return horizontal ? XDisplayWidth(tiny::get_display(), screen)
                      : XDisplayHeight(tiny::get_display(), screen);
}

int columns()
{
    const int fit = (screen_size(true) - 2 * spacing()) /
                    tiny::theme.wm_launcher_cell_width;
    return std::max(1, std::min(tiny::theme.wm_launcher_columns, fit));
}

int rows()
{
    // the launcher takes at most the three quarters of the screen
    const int free = screen_size(false) * 3 / 4 - 2 * spacing() -
                     entry_height() - 3 * tiny::theme.wm_launcher_dot;
    return std::max(1, std::min(tiny::theme.wm_launcher_rows,
                                free / tiny::theme.wm_launcher_cell_height));
}

int grid_width()
{
    return columns() * tiny::theme.wm_launcher_cell_width;
}

int grid_height()
{
    return tiny::IconGrid::height_of(rows(),
                                     tiny::theme.wm_launcher_cell_height,
                                     tiny::theme.wm_launcher_dot);
}

int dialog_width()
{
    return grid_width() + 2 * spacing();
}

int dialog_height()
{
    return spacing() + entry_height() + 2 * tiny::theme.entry.border_width +
           spacing() / 2 + grid_height() + spacing();
}

// The language of the names in the desktop files
std::string message_locale()
{
    for (const char* name : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* value = getenv(name);
        if (value != nullptr && *value != '\0') {
            return value;
        }
    }
    return "";
}

} // namespace

Launcher::Launcher():
    tiny::Popover(dialog_width(), dialog_height()),
    entry(grid_width() - 2 * tiny::theme.entry.border_width, entry_height()),
    grid(columns(), rows(), tiny::theme.wm_launcher_cell_width,
         tiny::theme.wm_launcher_cell_height, tiny::theme.wm_launcher_icon,
         tiny::theme.wm_launcher_dot)
{
    name = "launcher";
    entry.on_change.connect(
            this, static_cast<tiny::object_signal_t>(&Launcher::search));
    entry.on_activate.connect(
            this, static_cast<tiny::object_signal_t>(&Launcher::run));
    entry.on_cancel.connect(
            this, static_cast<tiny::object_signal_t>(&Launcher::close));
    entry.on_key.connect(this,
                         static_cast<tiny::object_signal_t>(&Launcher::move));
    grid.on_activate.connect(
            this, static_cast<tiny::object_signal_t>(&Launcher::run));
}

Launcher::~Launcher()
{
    for (auto& [id, icon] : icons) {
        if (icon.picture != None) {
            XRenderFreePicture(display, icon.picture);
        }
        if (icon.pixmap != None) {
            XFreePixmap(display, icon.pixmap);
        }
    }
}

void Launcher::realize(::Window root, int, int)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);
    root_window = root;

    tiny::Popover::realize(root, attrs.width / 2 - width / 2,
                           std::max(0, (attrs.height - height) / 4));

    push_start(&entry, spacing(), spacing());
    push_start(&grid, spacing(), spacing() / 2);
}

void Launcher::place()
{
    // the screen can change its size while the launcher is hidden
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, root_window, &attrs) != 0) {
        XMoveWindow(display, window, attrs.width / 2 - width / 2,
                    std::max(0, (attrs.height - height) / 4));
    }
}

void Launcher::popup()
{
    place();
    load();
    find();
    map_all();
    XRaiseWindow(display, window);
    entry.set_focus();
}

void Launcher::toggle()
{
    if (is_maped) {
        close(this, XEvent(), nullptr);
    } else {
        popup();
    }
}

void Launcher::close(Object*, const XEvent&, void*)
{
    entry.clear();
    unmap();
}

void Launcher::search(Object*, const XEvent&, void*)
{
    find();
}

void Launcher::move(Object*, const XEvent& e, void*)
{
    switch (XLookupKeysym(const_cast<XKeyEvent*>(&e.xkey), 0)) {
        case XK_Left:
            grid.select_prev();
            break;
        case XK_Right:
            grid.select_next();
            break;
        case XK_Up:
            grid.select_up();
            break;
        case XK_Down:
            grid.select_down();
            break;
        case XK_Page_Up:
            grid.select_page(false);
            break;
        case XK_Page_Down:
            grid.select_page(true);
            break;
        default:
            break;
    }
}

void Launcher::run(Object* o, const XEvent& e, void* data)
{
    if (found.empty()) {
        return;
    }
    const xdg::Application& app =
            index.applications()[found[grid.get_selected()]];
    if (xdg::launch(app) < 0) {
        // keep the launcher and the text, so the user can try again
        const int error = errno;
        const auto argv = xdg::expand_exec(app, {});
        tiny::error("Can't start %s (%s): %s", app.entry.name.c_str(),
                    argv.empty() ? "no command" : argv[0].c_str(),
                    strerror(error));
        return;
    }
    remember(app.id);
    close(o, e, data);
}

void Launcher::load()
{
    if (loaded) {
        return;
    }
    loaded = true;

    const char* data_home = getenv("XDG_DATA_HOME");
    const char* data_dirs = getenv("XDG_DATA_DIRS");
    const char* home = getenv("HOME");
    index.scan(xdg::application_dirs(data_home, data_dirs, home),
               message_locale());
    icon_theme = std::make_unique<xdg::IconTheme>(
            xdg::icon_dirs(data_home, data_dirs, home),
            xdg::pixmap_dirs(data_home, data_dirs, home));
}

void Launcher::find()
{
    found = xdg::search_applications(index.applications(), entry.get_text(),
                                     recent);
    grid.set_items(found.size(), [this](size_t row) { return item_of(row); });
}

tiny::IconGrid::Item Launcher::item_of(size_t row)
{
    const xdg::Application& app = index.applications()[found[row]];
    const Icon& icon = icon_of(app);

    tiny::IconGrid::Item item;
    item.text = app.entry.name;
    item.icon = icon.picture;
    item.icon_width = icon.width;
    item.icon_height = icon.height;
    return item;
}

const Launcher::Icon& Launcher::icon_of(const xdg::Application& app)
{
    auto it = icons.find(app.id);
    if (it == icons.end()) { // also an application with no icon, only once
        it = icons.emplace(app.id, load_icon(app.entry.icon)).first;
    }
    return it->second;
}

Launcher::Icon Launcher::load_icon(const std::string& icon_name) const
{
    Icon icon;
    const int size = tiny::theme.wm_launcher_icon;
    if (icon_name.empty() || !icon_theme) {
        return icon;
    }

    const std::string file = icon_theme->find(icon_name, size);
    if (file.empty()) {
        return icon;
    }
    const auto loaded_image = image::load_image(file, size);
    const auto scaled_image = loaded_image
                                      ? image::scale_image(*loaded_image, size)
                                      : std::nullopt;
    if (!scaled_image) {
        return icon;
    }

    icon.pixmap = image::create_pixmap(display, root_window, *scaled_image);
    if (icon.pixmap == None) {
        return icon;
    }
    icon.picture = image::create_argb_picture(display, icon.pixmap);
    icon.width = scaled_image->width;
    icon.height = scaled_image->height;
    return icon;
}

void Launcher::remember(const std::string& id)
{
    recent.erase(std::remove(recent.begin(), recent.end(), id), recent.end());
    recent.insert(recent.begin(), id);
    if (recent.size() > recent_count) {
        recent.resize(recent_count);
    }
}

} // namespace tiny::wm
