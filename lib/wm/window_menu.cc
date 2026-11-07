#include "wm/window_menu.h"

#include <utility>

#include "wm/window.h"

namespace tiny::wm {

WindowMenuState window_menu_state(const Window& window)
{
    WindowMenuState state;
    state.maximized = window.get_maximized();
    state.fullscreen = window.is_fullscreen();
    state.shaded = window.is_shaded();
    state.above = window.is_above();
    state.can_maximize = window.is_maximizable() && !window.is_auxiliary();
    state.can_minimize = window.is_minimizable();
    state.can_move = window.is_movable();
    state.can_resize = window.is_resizable();
    state.can_shade = window.is_shadable();
    state.can_close = window.is_closable();
    return state;
}

std::vector<WindowMenuItem> window_menu_items(const WindowMenuState& state)
{
    std::vector<WindowMenuItem> items;
    const auto add = [&](WindowMenuAction action, const char* label) {
        items.push_back({action, label});
    };
    // a fullscreen window covers everything, only the way out is left
    if (!state.fullscreen) {
        if (state.maximized && state.can_maximize) {
            add(WindowMenuAction::Restore, "Restore");
        } else if (!state.maximized && !state.shaded && state.can_maximize) {
            add(WindowMenuAction::Maximize, "Maximize");
        }
    }
    if (state.can_minimize) {
        add(WindowMenuAction::Minimize, "Minimize");
    }
    const bool free = !state.fullscreen && !state.maximized;
    if (free && state.can_move) {
        add(WindowMenuAction::Move, "Move");
    }
    if (free && !state.shaded && state.can_resize) {
        add(WindowMenuAction::Resize, "Resize");
    }
    if (free && state.can_shade) {
        add(state.shaded ? WindowMenuAction::RollDown
                         : WindowMenuAction::RollUp,
            state.shaded ? "Roll Down" : "Roll Up");
    }
    if (!state.fullscreen) {
        add(state.above ? WindowMenuAction::NotAlwaysOnTop
                        : WindowMenuAction::AlwaysOnTop,
            state.above ? "Not Always on Top" : "Always on Top");
    }
    if (state.can_close) {
        add(WindowMenuAction::Close, "Close");
    }
    return items;
}

void WindowMenu::realize(::Window root)
{
    menu.realize(root);
    menu.on_item = [this](size_t index) {
        Window* window = open_for;
        const bool valid = window != nullptr && index < items.size();
        const WindowMenuAction action =
                valid ? items[index].action : WindowMenuAction::Move;
        menu.unmap();
        if (valid && on_action) {
            on_action(*window, action);
        }
    };
    menu.on_hide = [this] { open_for = nullptr; };
}

void WindowMenu::popup(Window& window, int x_root, int y_root)
{
    std::vector<WindowMenuItem> offered =
            window_menu_items(window_menu_state(window));
    if (offered.empty()) { // a context menu is shown only when it has something
        return;
    }
    items = std::move(offered);
    menu.unmap();
    std::vector<std::string> labels;
    labels.reserve(items.size());
    for (const WindowMenuItem& item : items) {
        labels.push_back(item.label);
    }
    menu.set_items(labels);

    XWindowAttributes screen;
    XGetWindowAttributes(tiny::get_display(), menu.get_parent_window(),
                         &screen);
    const int border = 2 * menu.get_border();
    const MenuPlace place =
            place_menu(x_root, y_root, menu.content_width() + border,
                       menu.content_height() + border, false, false,
                       screen.width, screen.height);
    open_for = &window;
    // popup() puts the middle of the menu at x
    menu.popup(place.x + menu.content_width() / 2, place.y);
}

void WindowMenu::forget(const Window* window)
{
    if (open_for == window && window != nullptr) {
        menu.unmap();
        open_for = nullptr;
    }
}

} // namespace tiny::wm
