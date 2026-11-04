#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <utility>

#include "x_util.h"
#include "wm/dock.h"
#include "object.h"
#include "wm/window.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

namespace tiny::wm {

DockButton::DockButton(): DockButton(tiny::theme.wm_icon, tiny::theme.wm_icon)
{}

DockButton::DockButton(int w, int h): tiny::Widget(w, h)
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

DockLine::DockLine(const tiny::Style& style_arg, int w, int h):
    tiny::Widget(w, h), style(style_arg)
{
    name = "dock_line";
}

DockIcon::DockIcon(std::vector<Window*> windows_arg, std::string app_id_arg,
                   const Window::AppIcon& icon):
    DockButton(tiny::theme.wm_icon,
               tiny::theme.wm_icon + tiny::theme.wm_dock_border),
    windows(std::move(windows_arg)),
    app_id(std::move(app_id_arg)),
    favorite_icon(icon)
{
    if (app_id.empty() && !windows.empty()) {
        app_id = windows[0]->get_app_id();
    }
}

DockIcon::DockIcon(Window* task, std::string app_id_arg,
                   const Window::AppIcon& icon):
    DockIcon(task ? std::vector<Window*>{task} : std::vector<Window*>{},
             std::move(app_id_arg), icon)
{}

DockIcon::~DockIcon()
{
    if (event_done) {
        disconnect(ButtonPress);
        disconnect(MotionNotify);
    }
    if (target != None) {
        XRenderFreePicture(display, target);
    }
}

bool DockIcon::contains(const Window* window_arg) const
{
    return std::find(windows.begin(), windows.end(), window_arg) !=
           windows.end();
}

void DockIcon::set_events(long mask)
{
    DockButton::set_events(mask);

    // the same grab as the button has, with the press and the moves for a drag
    XGrabButton(display, Button1, AnyModifier, window, false,
                ButtonPressMask | ButtonReleaseMask | ButtonMotionMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(display, Button3, AnyModifier, window, false, ButtonReleaseMask,
                GrabModeAsync, GrabModeAsync, None, None);

    connect(ButtonPress, [this](const XEvent& e) { on_button_press(e); });
    connect(MotionNotify, [this](const XEvent& e) { on_motion(e); });
}

void DockIcon::on_button_press(const XEvent& e)
{
    pressed = draggable && e.xbutton.button == Button1;
    dragging = false;
    press_x = e.xbutton.x_root;
}

void DockIcon::on_motion(const XEvent& e)
{
    if (!pressed) {
        return;
    }
    const int x = e.xmotion.x_root;
    if (!dragging) {
        if (std::abs(x - press_x) < tiny::theme.wm_dock_drag) {
            return;
        }
        dragging = true;
        if (track) {
            track(1); // the icon must live until the drop
        }
        on_expose(e, nullptr); // smaller, it will be elsewhere
    }
    if (on_drag) {
        on_drag(*this, x);
    }
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

Window* DockIcon::next_window() const
{
    for (size_t i = 0; i < windows.size(); ++i) {
        if (windows[i]->is_focused()) {
            return windows[(i + 1) % windows.size()];
        }
    }
    return windows.empty() ? nullptr : windows[0];
}

void DockIcon::handle_release(const XEvent& e, void* data)
{
    if (e.xbutton.button == Button3) {
        on_menu(this, e);
        return;
    }
    const bool was_dragging = dragging;
    pressed = false;
    dragging = false;
    if (was_dragging) { // not a click
        on_expose(e, nullptr);
        if (on_drop) {
            on_drop(*this, e.xbutton.x_root);
        }
        if (track) {
            track(-1);
        }
        return;
    }
    if (Window* next = next_window()) {
        next->set_focus();
    }
    DockButton::on_button_release(e, data);
}

// The icon of the application is ARGB, so it is composited over the button;
// a small one is scaled to the half
void DockIcon::draw_app_icon(const Window::AppIcon& app_icon, bool small)
{
    if (target == None) { // the window has the default visual, made only once
        const XRenderPictFormat* format = XRenderFindVisualFormat(
                display, XDefaultVisual(display, XDefaultScreen(display)));
        if (format == nullptr) {
            return;
        }
        target = XRenderCreatePicture(display, window, format, 0, nullptr);
    }
    const int w = small ? app_icon.width / 2 : app_icon.width;
    const int h = small ? app_icon.height / 2 : app_icon.height;
    if (small) { // the transform maps the target to the source
        XTransform scale = {{{XDoubleToFixed(2), 0, 0},
                             {0, XDoubleToFixed(2), 0},
                             {0, 0, XDoubleToFixed(1)}}};
        XRenderSetPictureTransform(display, app_icon.picture, &scale);
    }
    XRenderComposite(display, PictOpOver, app_icon.picture, None, target, 0, 0,
                     0, 0, (width - w) / 2, (tiny::theme.wm_icon - h) / 2,
                     tiny::x_uint(w), tiny::x_uint(h));
    if (small) {
        XTransform identity = {{{XDoubleToFixed(1), 0, 0},
                                {0, XDoubleToFixed(1), 0},
                                {0, 0, XDoubleToFixed(1)}}};
        XRenderSetPictureTransform(display, app_icon.picture, &identity);
    }
}

void DockIcon::draw_dots(GC gc)
{
    const int dot = tiny::theme.wm_dock_dot;
    const int step = 2 * dot; // a dot and the same gap
    const size_t room = static_cast<size_t>(std::max(1, (width + dot) / step));
    const int count = static_cast<int>(std::min(windows.size(), room));
    const int left = (width - (count * step - dot)) / 2;
    // in the border of the dock under the icon, close to the edge
    const int top = tiny::theme.wm_icon +
                    std::max(0, tiny::theme.wm_dock_border - dot - 1);

    for (int i = 0; i < count; ++i) {
        const auto state = windows[static_cast<size_t>(i)]->is_focused()
                                   ? tiny::Style::State::Hover
                                   : tiny::Style::State::Normal;
        XSetForeground(display, gc,
                       tiny::theme.wm_dock.get_fg(static_cast<uint8_t>(state)));
        XFillArc(display, window, gc, left + i * step, top, tiny::x_uint(dot),
                 tiny::x_uint(dot), 0, 360 * 64);
    }
}

void DockIcon::on_expose(const XEvent&, void*)
{
    is_selected = std::any_of(windows.begin(), windows.end(),
                              [](const Window* w) { return w->is_focused(); });

    const int size = tiny::theme.wm_icon; // the dots are under it
    GC gc = XCreateGC(display, window, 0, nullptr);
    XSetWindowBackground(display, window,
                         get_style().get_bg(get_theme_state()));
    XClearWindow(display, window);

    const Window* task = get_task();
    const Pixmap icon = task ? task->get_icon() : None;
    const Pixmap icon_mask = task ? task->get_icon_mask() : None;
    const Window::AppIcon& shown = task && task->get_app_icon().picture != None
                                           ? task->get_app_icon()
                                           : favorite_icon;
    if (shown.picture != None) {
        draw_app_icon(shown, dragging);
    } else if (icon && !dragging) {
        if (icon_mask) {
            XGCValues gc_vals;
            gc_vals.clip_mask = icon_mask;
            XChangeGC(display, gc, GCClipMask, &gc_vals);
        }

        XCopyArea(display, icon, window, gc, 0, 0, tiny::x_uint(width),
                  tiny::x_uint(size), 0, 0);
    } else {
        // TODO: move to wm_window
        XSetForeground(display, gc, get_style().get_fg(get_theme_state()));
        XSetLineAttributes(display, gc, 2, LineSolid, CapButt, JoinBevel);

        const int d = dragging ? size / 2 : size;
        const int shift = (size - d) / 2;
        XDrawArc(display, window, gc, shift + 1, shift + 1, tiny::x_uint(d - 3),
                 tiny::x_uint(d - 3), 0, 360 * 64);
    }
    if (std::any_of(windows.begin(), windows.end(),
                    [](const Window* w) { return w->demands_attention(); })) {
        const int bar = tiny::theme.wm_dock_attention_bar;
        XSetForeground(display, gc, tiny::theme.wm_dock.attention);
        XFillRectangle(display, window, gc, 0, size - bar, tiny::x_uint(width),
                       tiny::x_uint(bar));
    }
    draw_dots(gc);
    XFreeGC(display, gc);
}

std::vector<DockGroup> group_dock(const std::vector<std::string>& favorites,
                                  const std::vector<std::string>& task_keys)
{
    std::vector<DockGroup> groups;
    std::vector<char> placed(task_keys.size(), 0);
    for (size_t i = 0; i < favorites.size(); ++i) {
        DockGroup group;
        group.favorite = static_cast<int>(i);
        group.key = favorites[i];
        for (size_t j = 0; j < task_keys.size(); ++j) {
            if (!placed[j] && task_keys[j] == favorites[i]) {
                placed[j] = 1;
                group.tasks.push_back(j);
            }
        }
        groups.push_back(std::move(group));
    }
    const size_t first_other = groups.size();
    for (size_t j = 0; j < task_keys.size(); ++j) {
        if (placed[j]) {
            continue;
        }
        const auto same = [&](const DockGroup& group) {
            return !task_keys[j].empty() && group.key == task_keys[j];
        };
        const auto found = std::find_if(
                groups.begin() + static_cast<std::ptrdiff_t>(first_other),
                groups.end(), same);
        if (found != groups.end()) {
            found->tasks.push_back(j);
        } else {
            groups.push_back({-1, task_keys[j], {j}});
        }
    }
    return groups;
}

size_t drop_position(const std::vector<int>& centers, int x)
{
    return static_cast<size_t>(std::count_if(centers.begin(), centers.end(),
                                             [x](int c) { return c < x; }));
}

std::string menu_title(std::string_view title, size_t max_chars)
{
    if (title.empty()) {
        return "Untitled";
    }
    size_t chars = 0;
    for (size_t i = 0; i < title.size(); ++i) {
        // a byte 10xxxxxx continues a character
        if ((static_cast<unsigned char>(title[i]) & 0xC0) == 0x80) {
            continue;
        }
        if (chars++ == max_chars) {
            return std::string(title.substr(0, i)) + "\xE2\x80\xA6";
        }
    }
    return std::string(title);
}

MenuPlace place_menu(int x, int y, int width, int height, bool above,
                     bool centered, int screen_width, int screen_height)
{
    const int left = centered ? x - width / 2 : x;
    const int top = above ? y - height : y;
    return {std::max(0, std::min(left, screen_width - width)),
            std::max(0, std::min(top, screen_height - height))};
}

DockMenu::DockMenu(): tiny::Popover(1, tiny::theme.wm_popup_item_height)
{
    name = "wm_dockmenu";
}

DockMenu::~DockMenu() = default;

void DockMenu::unmap()
{
    const bool was_maped = is_maped;
    tiny::Popover::unmap();
    if (was_maped && on_hide) {
        on_hide();
    }
}

void DockMenu::set_items(const std::vector<std::string>& labels)
{
    if (!is_realized) {
        return;
    }
    for (const auto& button : items) {
        remove(button.get());
    }
    items.clear();

    const int item_height = tiny::theme.wm_popup_item_height;
    const int padding = tiny::theme.wm_menu_padding;
    int text = 0;
    for (const std::string& label : labels) {
        items.push_back(
                std::make_unique<tiny::LabelButton>(1, item_height, label));
        text = std::max(text, items.back()->text_width());
    }
    content_w = text + 2 * padding;
    content_h = static_cast<int>(labels.size()) * item_height;

    for (size_t i = 0; i < items.size(); ++i) {
        tiny::LabelButton* button = items[i].get();
        button->resize(content_w, item_height); // not realized yet
        button->set_text_padding(padding);
        add(button, 0, static_cast<int>(i) * item_height);
        button->on_click.connect([this, i](const XEvent&) {
            if (on_item) {
                on_item(i);
            }
        });
        if (is_maped) {
            button->map_all();
        }
    }
    resize(content_w, content_h);
    width = content_w; // the server tells later, popup() needs it now
    height = content_h;
}

Dock::Dock():
    tiny::Box(tiny::Box::Type::Horizontal,
              tiny::theme.wm_icon + tiny::theme.wm_dock_border * 2,
              tiny::theme.wm_icon + tiny::theme.wm_dock_border * 2),
    mark(tiny::theme.wm_dock_mark, tiny::theme.wm_dock_marker,
         tiny::theme.wm_icon)
{
    name = "wm_dock";
    menu.on_item = [this](size_t index) { choose(index); };
    menu.on_hide = [this] {
        windows_menu.unmap();
        if (on_menu_closed) {
            on_menu_closed();
        }
    };
    menu.add_nested(&windows_menu);
    // a click outside of the submenu closes both
    windows_menu.on_hide = [this] { menu.unmap(); };
    windows_menu.on_item = [this](size_t index) {
        Window* chosen =
                index < menu_windows.size() ? menu_windows[index] : nullptr;
        hide_menus();
        if (chosen && contains(chosen)) {
            chosen->set_focus();
        }
    };
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
    if (std::find(menu_windows.begin(), menu_windows.end(), window_arg) !=
        menu_windows.end()) {
        hide_menus(); // the list of the windows is not true any more
    }
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

std::vector<Window*> Dock::windows_of(const std::string& app_id) const
{
    std::vector<Window*> found;
    std::copy_if(tasks.begin(), tasks.end(), std::back_inserter(found),
                 [&app_id](const Window* task) {
                     return task->get_app_id() == app_id;
                 });
    return found;
}

void Dock::clear()
{
    if (separator) {
        Box::remove(separator.get());
        separator.reset();
    }
    // the icons after the removed one move, so go from the last one
    for (auto it = icons.rbegin(); it != icons.rend(); ++it) {
        Box::remove(*it);
        delete *it;
    }
    icons.clear();
    icon_x.clear();
}

void Dock::track_handler(int change)
{
    handling += change;
    if (handling == 0 && rebuild_later) {
        rebuild_later = false;
        rebuild();
    }
}

namespace {

//! The windows of one application have one icon: its desktop file or, when
//! it is unknown, the class of the window
std::string task_key(const Window* task)
{
    if (!task->get_app_id().empty()) {
        return task->get_app_id();
    }
    if (!task->get_wm_class().empty()) {
        return "class:" + task->get_wm_class();
    }
    return {};
}

} // namespace

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
    std::vector<std::string> keys;
    keys.reserve(tasks.size());
    for (const Window* task : tasks) {
        keys.push_back(task_key(task));
    }

    const auto groups = group_dock(favorite_ids, keys);
    const int border = tiny::theme.wm_dock_border;
    const bool with_favorites = !favorites.empty();
    for (const DockGroup& group : groups) {
        if (with_favorites && group.favorite < 0 && !separator) {
            // not from edge to edge of the icons
            const int inset = tiny::theme.wm_icon / 6;
            separator = std::make_unique<DockLine>(
                    tiny::theme.wm_dock_separator, tiny::theme.wm_dock_line,
                    tiny::theme.wm_icon - 2 * inset);
            push_start(separator.get(), border, border + inset);
            separator->map();
        }
        std::vector<Window*> windows;
        windows.reserve(group.tasks.size());
        for (const size_t number : group.tasks) {
            windows.push_back(tasks[number]);
        }
        const DockFavorite* favorite =
                group.favorite < 0
                        ? nullptr
                        : &favorites[static_cast<size_t>(group.favorite)];
        auto* task_icon =
                favorite ? new DockIcon(windows, favorite->id, favorite->icon)
                         : new DockIcon(windows);
        task_icon->set_draggable(favorite != nullptr);
        task_icon->on_click.connect([this, task_icon](const XEvent&) {
            if (!task_icon->get_task() && on_launch) {
                on_launch(task_icon->get_app_id());
            }
        });
        task_icon->on_menu.connect([this, task_icon](const XEvent& e) {
            show_menu(*task_icon, e.xbutton);
        });
        task_icon->on_drag = [this](const DockIcon& dragged, int x_root) {
            drag(dragged, x_root);
        };
        task_icon->on_drop = [this](const DockIcon& dragged, int x_root) {
            drop(dragged, x_root);
        };
        task_icon->track = [this](int change) { track_handler(change); };
        icons.push_back(task_icon);
        icon_x.push_back(get_start_offset() + border);
        push_start(task_icon, border, border);
        task_icon->map_all();
    }
    resize_to_children();
}

size_t Dock::drop_place(const DockIcon& dragged, int x_root) const
{
    ::Window child;
    int x = 0;
    int y = 0;
    XTranslateCoordinates(display, window, parent, 0, 0, &x, &y, &child);

    std::vector<int> centers;
    for (size_t i = 0; i < icons.size(); ++i) {
        if (icons[i]->is_draggable() && icons[i] != &dragged) {
            centers.push_back(x + icon_x[i] + icons[i]->get_width() / 2);
        }
    }
    return drop_position(centers, x_root);
}

void Dock::drag(const DockIcon& dragged, int x_root)
{
    // the left edge of the mark: between the other favorites
    std::vector<int> edges; // left and right edge of every other favorite
    for (size_t i = 0; i < icons.size(); ++i) {
        if (icons[i]->is_draggable() && icons[i] != &dragged) {
            edges.push_back(icon_x[i]);
            edges.push_back(icon_x[i] + icons[i]->get_width());
        }
    }
    if (edges.empty()) { // nowhere else to go
        return;
    }
    const size_t place = drop_place(dragged, x_root);
    const int border = tiny::theme.wm_dock_border;
    int x;
    if (place == 0) {
        x = edges.front() - border / 2;
    } else if (place * 2 == edges.size()) {
        x = edges.back() + border / 2;
    } else {
        x = (edges[place * 2 - 1] + edges[place * 2]) / 2;
    }
    XMoveWindow(display, mark.get_window(), x - tiny::theme.wm_dock_marker / 2,
                border);
    mark.map();
    XRaiseWindow(display, mark.get_window());
}

void Dock::drop(const DockIcon& dragged, int x_root)
{
    mark.unmap();
    if (!on_move_favorite || !dragged.is_draggable()) {
        return;
    }
    size_t current = 0; // the place it has among the favorites
    for (const DockIcon* other : icons) {
        if (other == &dragged) {
            break;
        }
        current += other->is_draggable();
    }
    const size_t place = drop_place(dragged, x_root);
    if (place != current) {
        on_move_favorite(dragged.get_app_id(), place);
    }
}

void Dock::show_menu(const DockIcon& task_icon, const XButtonEvent& be)
{
    // above the icon, the dock is at the bottom of the screen
    open_menu(task_icon.get_app_id(), task_icon.get_windows(), be.x_root,
              be.y_root - be.y, true);
}

void Dock::popup_menu(const std::string& app_id, int x_root, int y_root)
{
    open_menu(app_id, windows_of(app_id), x_root, y_root, false);
}

void Dock::open_menu(const std::string& app_id, std::vector<Window*> windows,
                     int x, int y, bool above)
{
    if (app_id.empty() && windows.empty()) { // nothing to do with it
        return;
    }
    hide_menus();
    menu_app = app_id;
    menu_windows = std::move(windows);
    entries.clear();
    std::vector<std::string> labels;
    const auto item = [&](Item kind, std::string label, size_t number = 0) {
        entries.push_back({kind, number});
        labels.push_back(std::move(label));
    };

    const bool runs = !menu_windows.empty();
    if (!app_id.empty()) {
        item(Item::NewWindow, "New Window");
        if (list_actions) {
            size_t number = 0;
            for (std::string& action_name : list_actions(app_id)) {
                item(Item::Action, std::move(action_name), number++);
            }
        }
    }
    if (runs) {
        item(Item::Windows, "All Windows \xE2\x80\xBA");
    }
    if (!app_id.empty()) {
        item(Item::Favorite, is_favorite(app_id) ? "Remove from Favorites"
                                                 : "Add to Favorites");
    }
    if (runs) {
        item(Item::Close,
             menu_windows.size() == 1 ? "Close Window" : "Close All Windows");
    }
    menu.set_items(labels);

    XWindowAttributes screen;
    XGetWindowAttributes(display, parent, &screen);
    const int border = 2 * menu.get_border();
    menu_place = place_menu(x, y, menu.content_width() + border,
                            menu.content_height() + border, above, above,
                            screen.width, screen.height);
    // popup() puts the middle of the menu at x
    menu.popup(menu_place.x + menu.content_width() / 2, menu_place.y);
}

void Dock::choose(size_t index)
{
    if (index >= entries.size()) {
        hide_menus();
        return;
    }
    const Entry entry = entries[index];
    if (entry.kind == Item::Windows) {
        open_windows_menu(index);
        return;
    }
    const std::string app = menu_app;
    const std::vector<Window*> windows = menu_windows;
    hide_menus();
    switch (entry.kind) {
        case Item::NewWindow:
            if (on_launch) {
                on_launch(app);
            }
            break;
        case Item::Action:
            if (on_action) {
                on_action(app, entry.number);
            }
            break;
        case Item::Favorite:
            if (on_toggle_favorite) {
                on_toggle_favorite(app);
            }
            break;
        case Item::Close:
            for (Window* closed : windows) {
                if (contains(closed)) {
                    closed->close();
                }
            }
            break;
        case Item::Windows:
            break;
    }
}

void Dock::open_windows_menu(size_t index)
{
    std::vector<std::string> titles;
    titles.reserve(menu_windows.size());
    for (const Window* listed : menu_windows) {
        titles.push_back(menu_title(listed->get_wm_name(), 40));
    }
    windows_menu.set_items(titles);

    XWindowAttributes screen;
    XGetWindowAttributes(display, parent, &screen);
    const int border = 2 * windows_menu.get_border();
    const int w = windows_menu.content_width() + border;
    const int h = windows_menu.content_height() + border;
    const int menu_w = menu.content_width() + 2 * menu.get_border();
    // to the right of the item, or to the left when there is no room
    int x = menu_place.x + menu_w;
    if (x + w > screen.width) {
        x = menu_place.x - w;
    }
    const MenuPlace place =
            place_menu(x,
                       menu_place.y + static_cast<int>(index) *
                                              tiny::theme.wm_popup_item_height,
                       w, h, false, false, screen.width, screen.height);
    windows_menu.popup(place.x + windows_menu.content_width() / 2, place.y);
    // the catcher of the submenu covers the screen, the menu stays usable
    XRaiseWindow(display, menu.get_window());
    XRaiseWindow(display, windows_menu.get_window());
}

void Dock::hide_menus()
{
    windows_menu.unmap();
    menu.unmap();
}

void Dock::resize_to_children()
{
    resize(get_start_offset() + icon.get_width() +
                   2 * tiny::theme.wm_dock_border,
           height);
}

void Dock::redraw(const Window* window_arg, const XEvent& e)
{
    for (DockIcon* task_icon : icons) {
        if (task_icon->contains(window_arg)) {
            task_icon->redraw(e);
        }
    }
}

void Dock::realize(::Window parent_arg, int x, int y)
{
    tiny::Box::realize(parent_arg, x, y);

    push_back(&icon, tiny::theme.wm_dock_border, tiny::theme.wm_dock_border);
    mark.realize(window, 0, tiny::theme.wm_dock_border);
    menu.realize(parent_arg);
    windows_menu.realize(parent_arg);
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
