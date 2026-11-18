#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "spawn.h"
#include "theme.h"
#include "wm/shell_theme.h"
#include "wm/run_dialog.h"

namespace tiny::wm {

namespace {

constexpr int dialog_width = 320;

int spacing()
{
    return shell_theme.win_border;
}

int title_height()
{
    return shell_theme.menu_item_height;
}

// The border of the entry is outside of its size
int entry_height()
{
    return shell_theme.win_header;
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
    entry.on_activate.connect([this](tiny::Object* o, const XEvent& e,
                                     void* d) { run(o, e, d); });
    entry.on_cancel.connect([this](tiny::Object* o, const XEvent& e, void* d) {
        popover(o, e, d);
    });
    entry.on_tab.connect(
            [this](tiny::Object*, const XEvent&, void*) { complete(); });
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
    read_programs();
    if (screen.width > 0 && screen.height > 0) {
        XMoveWindow(display, window, screen.x + (screen.width - width) / 2,
                    screen.y + (screen.height - height) / 2);
    }
    map_all();
    XRaiseWindow(display, window);
    entry.set_focus();
}

void RunDialog::popover(Object*, const XEvent&, void*)
{
    entry.clear();
    unmap();
}

bool RunDialog::programs_ready() const
{
    return programs_taken ||
           (programs.valid() && programs.wait_for(std::chrono::seconds(0)) ==
                                        std::future_status::ready);
}

void RunDialog::read_programs()
{
    if (programs.valid()) {
        if (programs.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) {
            return; // still reading, the list is as fresh as it gets
        }
        programs = {}; // it was not used, read it again
    }
    programs_taken = false;
    if (list_programs) {
        programs = std::async(std::launch::async, list_programs);
        return;
    }
    // the environment is read here, getenv is not for other threads
    const char* path = getenv("PATH");
    programs = std::async(std::launch::async, list_commands,
                          std::string(path ? path : ""));
}

void RunDialog::complete()
{
    if (!programs_taken) {
        if (!programs_ready()) {
            return; // the list of the last showing is not offered
        }
        completer.set_commands(programs.get());
        programs_taken = true;
    }
    const std::string text = entry.get_text();
    const std::string completed = completer.next(text);
    if (completed != text) {
        entry.set_text(completed);
    }
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

} // namespace tiny::wm
