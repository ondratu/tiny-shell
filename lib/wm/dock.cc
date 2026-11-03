#include <algorithm>
#include <cmath>
#include <utility>

#include "x_util.h"
#include "wm/dock.h"
#include "object.h"
#include "wm/window.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

namespace tiny::wm {

DockButton::DockButton(): tiny::Widget(tiny::theme.wm_icon, tiny::theme.wm_icon)
{
    name = "dock_button";
}

DockButton::~DockButton()
{
    if (event_done) {
        XUngrabButton(display, Button1, AnyModifier, window);
        disconnect(Expose);
        disconnect(EnterNotify);
        disconnect(LeaveNotify);
        disconnect(ButtonRelease);
    }
}

void DockButton::set_events(long mask)
{
    tiny::Widget::set_events(ExposureMask | EnterWindowMask | LeaveWindowMask |
                             mask);

    XGrabButton(display, Button1, AnyModifier, window, false, ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);

    connect(EnterNotify,
            [this](const XEvent& e) { on_enter_notify(e, nullptr); });
    connect(LeaveNotify,
            [this](const XEvent& e) { on_leave_notify(e, nullptr); });
    connect(ButtonRelease,
            [this](const XEvent& e) { on_button_release(e, nullptr); });
    connect(Expose, [this](const XEvent& e) { on_expose(e, nullptr); });
}

void DockButton::on_enter_notify(const XEvent& e, void*)
{
    is_hover = true;
    on_expose(e, nullptr);
}

void DockButton::on_leave_notify(const XEvent& e, void*)
{
    is_hover = false;
    on_expose(e, nullptr);
}

void DockButton::on_button_release(const XEvent& e, void*)
{
    // TODO: check if release is on window, if not do not call the handler
    if (on_click) {
        on_click(this, e);
    }
}

void DockButton::on_expose(const XEvent&, void*)
{
    GC gc = XCreateGC(display, window, 0, nullptr);
    const uint8_t state = get_theme_state();

    XSetWindowBackground(display, window, get_style().get_bg(state));
    XClearWindow(display, window);

    XSetForeground(display, gc, get_style().get_fg(state));
    XSetLineAttributes(display, gc, 1, LineSolid, CapButt, JoinBevel);

    const int offset = width / 14;
    const unsigned int dot = static_cast<unsigned int>(
            std::max(1L, lround(5 * tiny::theme.scale_factor)));

    for (int y = 3; y <= 11; y += 4) {
        for (int x = 3; x <= 11; x += 4) {
            XFillArc(display, window, gc, x * offset, y * offset, dot, dot, 0,
                     360 * 64);
        }
    }
    XFreeGC(display, gc);
}

DockIcon::DockIcon(Window* task_arg, std::string app_id_arg,
                   const Window::AppIcon& icon):
    task(task_arg), app_id(std::move(app_id_arg)), favorite_icon(icon)
{
    if (app_id.empty() && task) {
        app_id = task->get_app_id();
    }
}

DockIcon::~DockIcon()
{
    if (target != None) {
        XRenderFreePicture(display, target);
    }
}

void DockIcon::set_events(long mask)
{
    DockButton::set_events(mask);

    XGrabButton(display, Button3, AnyModifier, window, false, ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);
}

void DockIcon::on_button_release(const XEvent& e, void* data)
{
    const auto done = track; // the icon may be deleted by the click
    if (done) {
        done(1);
    }
    handle_release(e, data);
    if (done) {
        done(-1);
    }
}

void DockIcon::handle_release(const XEvent& e, void* data)
{
    if (e.xbutton.button == Button3) {
        on_menu(this, e);
        return;
    }
    if (task) {
        task->set_focus();
    }
    DockButton::on_button_release(e, data);
}

// The icon of the application is ARGB, so it is composited over the button
void DockIcon::draw_app_icon(const Window::AppIcon& app_icon)
{
    if (target == None) { // the window has the default visual, made only once
        const XRenderPictFormat* format = XRenderFindVisualFormat(
                display, XDefaultVisual(display, XDefaultScreen(display)));
        if (format == nullptr) {
            return;
        }
        target = XRenderCreatePicture(display, window, format, 0, nullptr);
    }
    XRenderComposite(
            display, PictOpOver, app_icon.picture, None, target, 0, 0, 0, 0,
            (width - app_icon.width) / 2, (height - app_icon.height) / 2,
            tiny::x_uint(app_icon.width), tiny::x_uint(app_icon.height));
}

void DockIcon::on_expose(const XEvent&, void*)
{
    is_selected = task && task->is_focused();

    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetWindowBackground(display, window,
                         get_style().get_bg(get_theme_state()));
    XClearWindow(display, window);

    const Pixmap icon = task ? task->get_icon() : None;
    const Pixmap icon_mask = task ? task->get_icon_mask() : None;
    const Window::AppIcon& shown = task && task->get_app_icon().picture != None
                                           ? task->get_app_icon()
                                           : favorite_icon;
    if (shown.picture != None) {
        draw_app_icon(shown);
    } else if (icon) {
        if (icon_mask) {
            XGCValues gc_vals;
            gc_vals.clip_mask = icon_mask;
            XChangeGC(display, gc, GCClipMask, &gc_vals);
        }

        XCopyArea(display, icon, window, gc, 0, 0, tiny::x_uint(width),
                  tiny::x_uint(height), 0, 0);
    } else {
        // TODO: move to wm_window
        XSetForeground(display, gc, get_style().get_fg(get_theme_state()));
        XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

        XDrawArc(display, window, gc, 1, 1, tiny::x_uint(width - 3),
                 tiny::x_uint(height - 3), 0, 360 * 64);
    }
    if (task && task->demands_attention()) {
        const int bar = tiny::theme.wm_dock_attention_bar;
        XSetForeground(display, gc, tiny::theme.wm_dock.attention);
        XFillRectangle(display, window, gc, 0, height - bar,
                       tiny::x_uint(width), tiny::x_uint(bar));
    }
    XFreeGC(display, gc);
}

std::vector<DockSlot> arrange_dock(const std::vector<std::string>& favorites,
                                   const std::vector<std::string>& task_apps)
{
    std::vector<DockSlot> slots;
    std::vector<char> placed(task_apps.size(), 0);
    for (size_t i = 0; i < favorites.size(); ++i) {
        DockSlot slot;
        slot.favorite = static_cast<int>(i);
        for (size_t j = 0; j < task_apps.size(); ++j) {
            if (!placed[j] && task_apps[j] == favorites[i]) {
                placed[j] = 1;
                slot.task = static_cast<int>(j);
                break;
            }
        }
        slots.push_back(slot);
    }
    for (size_t j = 0; j < task_apps.size(); ++j) {
        if (!placed[j]) {
            slots.push_back({-1, static_cast<int>(j)});
        }
    }
    return slots;
}

DockMenu::DockMenu():
    tiny::Popover(tiny::theme.wm_dock_menu_width,
                  tiny::theme.wm_menu_item_height),
    toggle(tiny::theme.wm_dock_menu_width, tiny::theme.wm_menu_item_height,
           "Add to favorites")
{
    name = "wm_dockmenu";
}

DockMenu::~DockMenu() = default;

void DockMenu::realize(::Window parent_arg, int x, int y)
{
    tiny::Popover::realize(parent_arg, x, y);
    push_start(&toggle);
}

Dock::Dock():
    tiny::Box(tiny::Box::Type::Horizontal,
              tiny::theme.wm_icon + tiny::theme.wm_dock_border * 2,
              tiny::theme.wm_icon + tiny::theme.wm_dock_border * 2)
{
    name = "wm_dock";
    menu.toggle.on_click.connect([this](const XEvent&) {
        menu.unmap();
        if (on_toggle_favorite && !menu_app.empty()) {
            on_toggle_favorite(menu_app);
        }
    });
}

Dock::~Dock()
{
    for (const DockIcon* task_icon : icons) {
        delete task_icon;
    }
    icons.clear();
}

void Dock::add(Window* window_arg)
{
    if (contains(window_arg)) {
        return;
    }
    tasks.push_back(window_arg);
    rebuild();
}

void Dock::remove(Window* window_arg)
{
    const auto it = std::find(tasks.begin(), tasks.end(), window_arg);
    if (it == tasks.end()) {
        return;
    }
    tasks.erase(it);
    rebuild();
}

void Dock::set_favorites(std::vector<DockFavorite> list)
{
    favorites = std::move(list);
    rebuild();
}

bool Dock::contains(const Window* window_arg) const
{
    return std::find(tasks.begin(), tasks.end(), window_arg) != tasks.end();
}

bool Dock::is_favorite(const std::string& id) const
{
    return std::any_of(
            favorites.begin(), favorites.end(),
            [&id](const DockFavorite& favorite) { return favorite.id == id; });
}

void Dock::clear()
{
    // the icons after the removed one move, so go from the last one
    for (auto it = icons.rbegin(); it != icons.rend(); ++it) {
        Box::remove(*it);
        delete *it;
    }
    icons.clear();
}

void Dock::track_handler(int change)
{
    handling += change;
    if (handling == 0 && rebuild_later) {
        rebuild_later = false;
        rebuild();
    }
}

void Dock::rebuild()
{
    if (handling > 0) { // the icon of the click is still working
        rebuild_later = true;
        return;
    }
    if (!is_realized) { // the icons are made when the dock is realized
        return;
    }
    clear();

    std::vector<std::string> favorite_ids;
    favorite_ids.reserve(favorites.size());
    for (const DockFavorite& favorite : favorites) {
        favorite_ids.push_back(favorite.id);
    }
    std::vector<std::string> task_apps;
    task_apps.reserve(tasks.size());
    for (const Window* task : tasks) {
        task_apps.push_back(task->get_app_id());
    }

    for (const DockSlot& slot : arrange_dock(favorite_ids, task_apps)) {
        Window* task =
                slot.task < 0 ? nullptr : tasks[static_cast<size_t>(slot.task)];
        const DockFavorite* favorite =
                slot.favorite < 0
                        ? nullptr
                        : &favorites[static_cast<size_t>(slot.favorite)];
        auto* task_icon =
                favorite ? new DockIcon(task, favorite->id, favorite->icon)
                         : new DockIcon(task);
        task_icon->on_click.connect([this, task_icon](const XEvent&) {
            if (!task_icon->get_task() && on_launch) {
                on_launch(task_icon->get_app_id());
            }
        });
        task_icon->on_menu.connect([this, task_icon](const XEvent& e) {
            show_menu(*task_icon, e.xbutton);
        });
        task_icon->track = [this](int change) { track_handler(change); };
        icons.push_back(task_icon);
        push_start(task_icon, tiny::theme.wm_dock_border,
                   tiny::theme.wm_dock_border);
        task_icon->map_all();
    }
    resize_to_children();
}

void Dock::show_menu(const DockIcon& task_icon, const XButtonEvent& be)
{
    if (task_icon.get_app_id().empty()) { // no desktop file to remember
        return;
    }
    menu_app = task_icon.get_app_id();
    menu.toggle.set_text(is_favorite(menu_app) ? "Remove from favorites"
                                               : "Add to favorites");
    // above the icon, the dock is at the bottom of the screen
    menu.popup(be.x_root,
               be.y_root - be.y - menu.get_height() - 2 * menu.get_border());
}

void Dock::resize_to_children()
{
    const int size = tiny::theme.wm_icon + tiny::theme.wm_dock_border;
    resize(static_cast<int>(children.size()) * size +
                   tiny::theme.wm_dock_border,
           height);
}

void Dock::redraw(const Window* window_arg, const XEvent& e)
{
    for (DockIcon* task_icon : icons) {
        if (task_icon->get_task() == window_arg) {
            task_icon->redraw(e);
        }
    }
}

void Dock::realize(::Window parent_arg, int x, int y)
{
    tiny::Box::realize(parent_arg, x, y);

    push_back(&icon, tiny::theme.wm_dock_border, tiny::theme.wm_dock_border);
    menu.realize(parent_arg);
    if (!tasks.empty() || !favorites.empty()) {
        rebuild(); // they came before the dock; it is moved by its size
    }
}

void Dock::resize(int w, int h)
{
    Box::resize(w, h);

    if (is_realized) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, parent, &attrs);
        XMoveWindow(display, window, attrs.width / 2 - w / 2,
                    attrs.height - h - tiny::theme.wm_dock_border);
    }
}

} // namespace tiny::wm
