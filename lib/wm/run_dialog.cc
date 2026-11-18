#include <cerrno>
#include <pthread.h>

#include <atomic>
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

struct RunDialog::Reading {
    std::function<std::vector<std::string>()> job;
    std::vector<std::string> result; //!< read after `done`
    std::atomic<bool> done{false};
};

void* RunDialog::read_in_thread(void* arg)
{
    auto* ref = static_cast<std::shared_ptr<Reading>*>(arg);
    const std::shared_ptr<Reading> reading = *ref;
    delete ref;
    try {
        reading->result = reading->job();
    } catch (...) {
        reading->result.clear(); // no programs, the user types the name
    }
    reading->done.store(true, std::memory_order_release);
    return nullptr;
}

bool RunDialog::programs_ready() const
{
    return programs_taken ||
           (reading && reading->done.load(std::memory_order_acquire));
}

void RunDialog::read_programs()
{
    if (reading) {
        if (!reading->done.load(std::memory_order_acquire)) {
            return; // still reading, the list is as fresh as it gets
        }
        reading.reset(); // it was not used, read it again
    }
    programs_taken = false;
    auto next = std::make_shared<Reading>();
    if (list_programs) {
        next->job = list_programs;
    } else {
        // the environment is read here, getenv is not for other threads
        const char* path = getenv("PATH");
        next->job = [path = std::string(path ? path : "")] {
            return list_commands(path);
        };
    }
    pthread_t thread;
    auto* ref = new std::shared_ptr<Reading>(next);
    if (pthread_create(&thread, nullptr, read_in_thread, ref) != 0) {
        delete ref;
        return; // no thread: no completion, the dialog works
    }
    pthread_detach(thread);
    reading = std::move(next);
}

void RunDialog::complete()
{
    if (!programs_taken) {
        if (!programs_ready()) {
            return; // the list of the last showing is not offered
        }
        completer.set_commands(std::move(reading->result));
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
