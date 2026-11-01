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

void fit(tiny::Widget& widget, const tiny::Rect& r)
{
    widget.resize(r.width, r.height);
    if (widget.get_realized()) {
        XMoveWindow(tiny::get_display(), widget.get_window(), r.x, r.y);
    }
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
    tiny::Popover(1, 1),
    entry(1, tiny::theme.wm_win_header),
    grid(1, 1, 1, 1, 1, tiny::theme.wm_launcher_dot),
    prev_page(1, 1, "<"),
    next_page(1, 1, ">")
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
    prev_page.on_click.connect(
            this, static_cast<tiny::object_signal_t>(&Launcher::turn));
    next_page.on_click.connect(
            this, static_cast<tiny::object_signal_t>(&Launcher::turn));
}

Launcher::~Launcher()
{
    free_icons();
}

void Launcher::realize(::Window root, int, int)
{
    root_window = root;
    relayout(); // the sizes are known before the windows are created

    tiny::Popover::realize(root, 0, 0);
    XSetWindowBorderWidth(display, window, 0);

    add(&entry, layout.entry.x, layout.entry.y);
    add(&grid, layout.grid.x, layout.grid.y);
    add(&prev_page, layout.prev.x, layout.prev.y);
    add(&next_page, layout.next.x, layout.next.y);
}

// The screen can change its size while the launcher is hidden
void Launcher::relayout()
{
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, root_window, &attrs) == 0) {
        return;
    }
    const LauncherLayout next =
            LauncherLayout::compute(attrs.width, attrs.height, tiny::theme);
    if (next.icon != layout.icon) {
        free_icons(); // loaded in the other size
    }
    arrange(next);
}

void Launcher::arrange(const LauncherLayout& next)
{
    layout = next;
    width = layout.screen.width;
    height = layout.screen.height;
    if (is_realized) {
        XMoveResizeWindow(display, window, 0, 0, tiny::x_dim(width),
                          tiny::x_dim(height));
    }
    fit(entry, layout.entry);
    if (grid.get_realized()) {
        XMoveWindow(display, grid.get_window(), layout.grid.x, layout.grid.y);
    }
    // the size and the redraw, after the grid is in its place
    grid.set_geometry(layout.columns, layout.rows, layout.cell_width,
                      layout.cell_height, layout.icon);
    fit(prev_page, layout.prev);
    fit(next_page, layout.next);
}

// The buttons of the pages are mapped only when there are pages
void Launcher::map_all()
{
    map();
    entry.map_all();
    grid.map_all();
    update_buttons();
}

void Launcher::update_buttons()
{
    if (!is_maped) {
        return;
    }
    if (grid.page_count() > 1) {
        prev_page.map();
        next_page.map();
    } else {
        prev_page.unmap();
        next_page.unmap();
    }
}

void Launcher::free_icons()
{
    for (auto& [id, icon] : icons) {
        if (icon.picture != None) {
            XRenderFreePicture(display, icon.picture);
        }
        if (icon.pixmap != None) {
            XFreePixmap(display, icon.pixmap);
        }
    }
    icons.clear();
}

void Launcher::popup()
{
    relayout();
    load();
    find();
    map_all();
    XRaiseWindow(display, window);
    for (const tiny::Widget* widget : above) {
        XRaiseWindow(display, widget->get_window());
    }
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

void Launcher::turn(Object* o, const XEvent&, void*)
{
    grid.select_page(o == &next_page);
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
    update_buttons();
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
    const int size = layout.icon;
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
