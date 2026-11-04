#include "wm/unresponsive_dialog.h"

#include <algorithm>

#include "theme.h"
#include "wm/shell_theme.h"

namespace tiny::wm {

namespace {

constexpr int max_name = 48; //!< bytes of the name of the application

int dialog_width()
{
    return 20 * shell_theme.menu_item_height;
}

// The name is the client's, so it may be anything: cut it on a character
std::string shorten(const std::string& name)
{
    if (name.size() <= max_name) {
        return name;
    }
    size_t end = max_name;
    while (end > 0 && (name[end] & 0xC0) == 0x80) {
        --end; // inside of a UTF-8 sequence
    }
    return name.substr(0, end) + "...";
}

int spacing()
{
    return shell_theme.win_border;
}

int line_height()
{
    return shell_theme.menu_item_height;
}

int button_height()
{
    return shell_theme.win_header;
}

int button_width()
{
    return (dialog_width() - 3 * spacing()) / 2;
}

int dialog_height()
{
    return spacing() + 2 * line_height() + spacing() + button_height() +
           spacing();
}

} // namespace

UnresponsiveDialog::UnresponsiveDialog():
    tiny::Container(dialog_width(), dialog_height(), "unresponsive-dialog"),
    title(dialog_width() - 2 * spacing(), line_height()),
    detail(dialog_width() - 2 * spacing(), line_height()),
    wait(button_width(), button_height(), "Wait"),
    quit(button_width(), button_height(), "Force quit")
{
    wait.on_click.connect([this](const XEvent& e) { on_wait(this, e); });
    quit.on_click.connect([this](const XEvent& e) { on_quit(this, e); });
}

UnresponsiveDialog::~UnresponsiveDialog() = default;

void UnresponsiveDialog::realize(::Window root, int, int)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    tiny::Widget::realize(root, attrs.width / 2 - width / 2,
                          attrs.height / 2 - height / 2);
    XSetWindowAttributes set;
    set.override_redirect = true; // the dialog is ours, not a client
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &set);

    const int lines_y = spacing();
    const int buttons_y = lines_y + 2 * line_height() + spacing();
    add(&title, spacing(), lines_y);
    add(&detail, spacing(), lines_y + line_height());
    add(&wait, spacing(), buttons_y);
    add(&quit, 2 * spacing() + button_width(), buttons_y);
}

void UnresponsiveDialog::place_over(::Window frame)
{
    XWindowAttributes around;
    XWindowAttributes root;
    if (!XGetWindowAttributes(display, frame, &around) ||
        !XGetWindowAttributes(display, around.root, &root)) {
        return;
    }
    const int x = std::clamp(around.x + (around.width - width) / 2, 0,
                             std::max(root.width - width, 0));
    const int y = std::clamp(around.y + (around.height - height) / 2, 0,
                             std::max(root.height - height, 0));
    XMoveWindow(display, window, x, y);

    XWindowChanges changes = {};
    changes.sibling = frame;
    changes.stack_mode = Above;
    XConfigureWindow(display, window, CWSibling | CWStackMode, &changes);
}

void UnresponsiveDialog::popup(const std::string& app, unsigned others)
{
    title.set_text("\"" + shorten(app) + "\" is not responding");
    detail.set_text(others == 0 ? "You may wait, or force it to quit."
                                : "Quitting closes " + std::to_string(others) +
                                          " more window" +
                                          (others == 1 ? "" : "s") + " of it.");
    map_all();
}

} // namespace tiny::wm
