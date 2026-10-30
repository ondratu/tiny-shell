#include <cerrno>
#include <cstring>

#include "spawn.h"
#include "theme.h"
#include "wm_run.h"

namespace wm {

namespace {

constexpr int dialog_width = 320;

int spacing()
{
    return tiny::theme.wm_win_border;
}

int title_height()
{
    return tiny::theme.wm_menu_item_height;
}

// The border of the entry is outside of its size
int entry_height()
{
    return tiny::theme.wm_win_header;
}

int dialog_height()
{
    return spacing() + title_height() + spacing() / 2 + entry_height() +
           2 * tiny::theme.entry.border_width + spacing();
}

} // namespace

RunDialog::RunDialog():
    tiny::Popover(dialog_width, dialog_height()),
    title(dialog_width - 2 * spacing(), title_height(), "Run a command"),
    entry(dialog_width - 2 * spacing() - 2 * tiny::theme.entry.border_width,
          entry_height())
{
    name = "run-dialog";
    entry.on_activate.connect(
            this, static_cast<tiny::object_signal_t>(&RunDialog::run));
    entry.on_cancel.connect(
            this, static_cast<tiny::object_signal_t>(&RunDialog::popover));
}

RunDialog::~RunDialog() = default;

void RunDialog::realize(::Window root, int, int)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    tiny::Popover::realize(root, attrs.width / 2 - width / 2,
                           attrs.height / 2 - height / 2);

    push_start(&title, spacing(), spacing());
    push_start(&entry, spacing(), spacing() / 2);
}

void RunDialog::popup()
{
    map_all();
    XRaiseWindow(display, window);
    entry.set_focus();
}

void RunDialog::popover(Object*, const XEvent&, void*)
{
    entry.clear();
    unmap();
}

void RunDialog::run(Object* o, const XEvent& e, void* data)
{
    if (!entry.get_text().empty() &&
        tiny::spawn_command(entry.get_text()) < 0) {
        // keep the dialog and the text, so the user can try again
        tiny::error("Can't run the command: %s", strerror(errno));
        return;
    }
    popover(o, e, data);
}

} // namespace wm
