#include "wm/about_dialog.h"

#include "wm/shell_theme.h"

namespace tiny::wm {

namespace {

int dialog_width()
{
    return 20 * shell_theme.menu_item_height;
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
    return 5 * shell_theme.menu_item_height;
}

int dialog_height()
{
    return spacing() + AboutDialog::line_count * line_height() + spacing() +
           button_height() + spacing();
}

} // namespace

AboutDialog::AboutDialog():
    tiny::Popover(dialog_width(), dialog_height()),
    title(dialog_width() - 2 * spacing(), line_height(), "Tiny Shell"),
    lines{{dialog_width() - 2 * spacing(), line_height()},
          {dialog_width() - 2 * spacing(), line_height()},
          {dialog_width() - 2 * spacing(), line_height()},
          {dialog_width() - 2 * spacing(), line_height()}},
    close(button_width(), button_height(), "Close")
{
    name = "about-dialog";
    close.on_click.connect([this](const XEvent&) { unmap(); });
}

AboutDialog::~AboutDialog() = default;

void AboutDialog::realize(::Window root, int, int)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    tiny::Popover::realize(root, attrs.width / 2 - width / 2,
                           attrs.height / 2 - height / 2);

    push_start(&title, spacing(), spacing());
    for (tiny::Label& label : lines) {
        push_start(&label, spacing(), 0);
    }
    add(&close, (dialog_width() - button_width()) / 2,
        height - spacing() - button_height());
}

void AboutDialog::popup(const std::string& version, const SystemInfo& info)
{
    lines[0].set_text("A small window manager and shell for X11");
    lines[1].set_text("Version " + version);
    lines[2].set_text(info.name.empty() ? "System unknown" : info.name);
    lines[3].set_text(info.kernel.empty()
                              ? ""
                              : info.kernel +
                                        (info.machine.empty()
                                                 ? ""
                                                 : " (" + info.machine + ")"));
    map_all();
    XRaiseWindow(display, window);
}

const std::string& AboutDialog::line(int index) const
{
    static const std::string none;
    if (index == 0) {
        return title.get_text();
    }
    return index > 0 && index < line_count
                   ? lines[static_cast<size_t>(index) - 1].get_text()
                   : none;
}

} // namespace tiny::wm
