#include "wm/unresponsive_dialog.h"

#include "theme.h"

namespace tiny::wm {

namespace {

constexpr int dialog_width = 400;

int spacing()
{
    return tiny::theme.wm_win_border;
}

int line_height()
{
    return tiny::theme.wm_menu_item_height;
}

int button_height()
{
    return tiny::theme.wm_win_header;
}

int button_width()
{
    return (dialog_width - 3 * spacing()) / 2;
}

int dialog_height()
{
    return spacing() + 2 * line_height() + spacing() + button_height() +
           spacing();
}

} // namespace

UnresponsiveDialog::UnresponsiveDialog():
    tiny::Container(dialog_width, dialog_height(), "unresponsive-dialog"),
    title(dialog_width - 2 * spacing(), line_height()),
    detail(dialog_width - 2 * spacing(), line_height()),
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

void UnresponsiveDialog::popup(const std::string& name, unsigned others)
{
    title.set_text("\"" + name + "\" is not responding");
    detail.set_text(others == 0 ? "You may wait, or force it to quit."
                                : "Quitting closes " + std::to_string(others) +
                                          " more window" +
                                          (others == 1 ? "" : "s") + " of it.");
    map_all();
    XRaiseWindow(display, window);
}

} // namespace tiny::wm
