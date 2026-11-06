#include "wm/switcher.h"

#include <algorithm>

#include "wm/dock.h"

namespace tiny::wm {

Switcher::Switcher():
    apps(shell_theme.icon, shell_theme.switcher_cell_width),
    windows(shell_theme.switcher_window_icon,
            shell_theme.switcher_window_cell_width)
{}

Switcher::~Switcher() = default;

void Switcher::realize(::Window root_arg)
{
    root = root_arg;
    apps.realize(root, 0, 0);
    windows.realize(root, 0, 0);
}

void Switcher::set_icons(SymbolicIcons* icons)
{
    apps.set_icons(icons);
    windows.set_icons(icons);
}

void Switcher::open(const std::vector<Window*>& windows_arg, bool by_windows,
                    bool backwards)
{
    if (windows_arg.empty() || !apps.get_realized()) {
        return;
    }
    list = windows_arg;
    std::vector<std::string> keys;
    keys.reserve(list.size());
    for (const Window* window : list) {
        keys.push_back(window->get_app_id());
    }
    model.emplace(keys);
    if (by_windows) {
        model->start_windows(backwards);
    } else {
        model->start_apps(backwards);
    }

    XWindowAttributes attrs;
    XGetWindowAttributes(tiny::get_display(), root, &attrs);
    screen_width = attrs.width;
    screen_height = attrs.height;

    std::vector<SwitcherCell> cells;
    for (const DockGroup& group : model->apps()) {
        const Window& first = *list[group.tasks.front()];
        SwitcherCell cell =
                icon_of ? icon_of(first, shell_theme.icon) : SwitcherCell();
        cell.text = app_name ? app_name(first) : std::string();
        cell.dots = group.tasks.size();
        cells.push_back(std::move(cell));
    }
    // a margin of a panel at both sides of the screen
    const int room = screen_width - 4 * shell_theme.dock_border;
    apps.set_max_width(room);
    windows.set_max_width(room);
    windows_of.reset();
    apps.set_cells(std::move(cells), model->get_app());
    refresh();
    apps.map();
    XRaiseWindow(tiny::get_display(), apps.get_window());
}

void Switcher::show_windows()
{
    if (windows_of != model->get_app()) {
        std::vector<SwitcherCell> cells;
        for (const size_t task : model->app_windows()) {
            const Window& window = *list[task];
            SwitcherCell cell =
                    icon_of ? icon_of(window, shell_theme.switcher_window_icon)
                            : SwitcherCell();
            cell.text = menu_title(window.get_wm_name(), 200);
            cells.push_back(std::move(cell));
        }
        windows_of = model->get_app();
        windows.set_cells(std::move(cells), model->get_window());
    } else {
        windows.select(model->get_window());
    }
}

void Switcher::refresh()
{
    apps.select(model->get_app());
    if (model->get_level() == SwitcherModel::Level::Windows) {
        show_windows();
        place();
        windows.map();
        XRaiseWindow(tiny::get_display(), windows.get_window());
    } else {
        windows.unmap();
        place();
    }
}

void Switcher::place()
{
    const int gap = shell_theme.dock_border;
    // the panel of the applications does not move when the windows come
    const int apps_top = (screen_height - apps.get_height()) / 2;
    apps.place(screen_width / 2, std::max(0, apps_top));
    const int below = apps_top + apps.get_height() + gap;
    const int fits = screen_height - windows.get_height() - gap;
    windows.place(screen_width / 2, std::max(0, std::min(below, fits)));
}

void Switcher::next_app(bool backwards)
{
    if (!model) {
        return;
    }
    model->next_app(backwards);
    refresh();
}

void Switcher::arrow(Arrow direction)
{
    if (!model) {
        return;
    }
    switch (direction) {
        case Arrow::Left:
            model->move(true);
            break;
        case Arrow::Right:
            model->move(false);
            break;
        case Arrow::Up:
            model->up();
            break;
        case Arrow::Down:
            model->down();
            break;
    }
    refresh();
}

void Switcher::next_window(bool backwards)
{
    if (!model) {
        return;
    }
    model->next_window(backwards);
    refresh();
}

Window* Switcher::close()
{
    if (!model) {
        return nullptr;
    }
    Window* chosen = nullptr;
    if (const auto result = model->result()) {
        chosen = list[*result];
    }
    model.reset();
    list.clear();
    apps.unmap();
    windows.unmap();
    windows_of.reset();
    return chosen;
}

bool Switcher::contains(const Window* window) const
{
    return std::find(list.begin(), list.end(), window) != list.end();
}

} // namespace tiny::wm
