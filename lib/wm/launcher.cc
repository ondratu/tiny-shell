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

int row_height()
{
    return tiny::theme.wm_launcher_icon + 2 * tiny::theme.list.padding_width;
}

int list_height()
{
    return tiny::theme.wm_launcher_rows * row_height();
}

int inner_width()
{
    return tiny::theme.wm_launcher_width - 2 * spacing();
}

int dialog_height()
{
    return spacing() + entry_height() + 2 * tiny::theme.entry.border_width +
           spacing() / 2 + list_height() + spacing();
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
    tiny::Popover(tiny::theme.wm_launcher_width, dialog_height()),
    entry(inner_width() - 2 * tiny::theme.entry.border_width, entry_height()),
    list(inner_width(), list_height(), row_height(),
         tiny::theme.wm_launcher_icon)
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
    list.on_activate.connect(
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
    push_start(&list, spacing(), spacing() / 2);
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
        case XK_Up:
            list.select_prev();
            break;
        case XK_Down:
            list.select_next();
            break;
        case XK_Page_Up:
            list.select_page(false);
            break;
        case XK_Page_Down:
            list.select_page(true);
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
            index.applications()[found[list.get_selected()]];
    if (xdg::launch(app) < 0) {
        // keep the launcher and the text, so the user can try again
        tiny::error("Can't start %s: %s", app.entry.name.c_str(),
                    strerror(errno));
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
    list.set_items(found.size(), [this](size_t row) { return item_of(row); });
}

tiny::ListBox::Item Launcher::item_of(size_t row)
{
    const xdg::Application& app = index.applications()[found[row]];
    const Icon& icon = icon_of(app);

    tiny::ListBox::Item item;
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
