#include "wm/session_dialog.h"

#include "theme.h"
#include "wm/shell_theme.h"

namespace tiny::wm {

namespace {

constexpr int max_text = 64; //!< bytes of a line, what is more is cut

int dialog_width()
{
    return 28 *
           shell_theme.menu_item_height; // the longest line is about 60 letters
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
    return spacing() + 3 * line_height() + spacing() + button_height() +
           spacing();
}

// The names are the clients', so they may be anything: cut it on a character
std::string shorten(const std::string& text)
{
    if (text.size() <= max_text) {
        return text;
    }
    size_t end = max_text;
    while (end > 0 && (text[end] & 0xC0) == 0x80) {
        --end; // inside of a UTF-8 sequence
    }
    return text.substr(0, end) + "...";
}

} // namespace

SessionDialog::SessionDialog():
    tiny::Container(dialog_width(), dialog_height(), "session-dialog"),
    title(dialog_width() - 2 * spacing(), line_height()),
    first(dialog_width() - 2 * spacing(), line_height()),
    second(dialog_width() - 2 * spacing(), line_height()),
    cancel(button_width(), button_height(), "Cancel"),
    proceed(button_width(), button_height(), "Log Out")
{
    cancel.on_click.connect([this](const XEvent& e) { on_cancel(this, e); });
    proceed.on_click.connect([this](const XEvent& e) { on_proceed(this, e); });
}

SessionDialog::~SessionDialog() = default;

void SessionDialog::realize(::Window root, int, int)
{
    XWindowAttributes attrs;
    XGetWindowAttributes(display, root, &attrs);

    tiny::Widget::realize(root, attrs.width / 2 - width / 2,
                          attrs.height / 2 - height / 2);
    XSetWindowAttributes set;
    set.override_redirect = true; // the dialog is ours, not a client
    XChangeWindowAttributes(display, window, CWOverrideRedirect, &set);

    const int lines_y = spacing();
    const int buttons_y = lines_y + 3 * line_height() + spacing();
    add(&title, spacing(), lines_y);
    add(&first, spacing(), lines_y + line_height());
    add(&second, spacing(), lines_y + 2 * line_height());
    add(&cancel, spacing(), buttons_y);
    add(&proceed, 2 * spacing() + button_width(), buttons_y);
}

void SessionDialog::set_screen(const tiny::Rect& monitor)
{
    screen = monitor;
    if (get_realized() && get_maped()) {
        place();
    }
}

void SessionDialog::place()
{
    if (screen.width > 0 && screen.height > 0) {
        XMoveWindow(display, window, screen.x + (screen.width - width) / 2,
                    screen.y + (screen.height - height) / 2);
    }
}

void SessionDialog::set_buttons(const std::string& cancel_text,
                                const std::string& proceed_text)
{
    if (cancel.get_text() != cancel_text) {
        cancel.set_text(cancel_text);
    }
    if (proceed_text.empty()) {
        proceed.unmap(); // only Close is left
    } else {
        if (proceed.get_text() != proceed_text) {
            proceed.set_text(proceed_text);
        }
        if (get_maped() && !proceed.get_maped()) {
            proceed.map_all();
        }
    }
}

void SessionDialog::set_lines(const std::string& new_title,
                              const std::string& line1,
                              const std::string& line2)
{
    const bool shown = get_maped();
    // the countdown asks again every moment: a text that is there is not drawn
    if (title.get_text() != new_title) {
        title.set_text(new_title);
    }
    if (first.get_text() != shorten(line1)) {
        first.set_text(shorten(line1));
    }
    if (second.get_text() != shorten(line2)) {
        second.set_text(shorten(line2));
    }
    if (!shown) {
        place();
        map_all();
        // above the windows; a dialog that an application opens later is
        // above this one, and the user can still get to it
        XRaiseWindow(display, window);
    }
}

void SessionDialog::show_question(SessionKind kind, int seconds)
{
    set_buttons("Cancel", session_proceed_label(kind, false));
    set_lines(session_title(kind), session_question(kind, seconds), "");
}

void SessionDialog::show_waiting(SessionKind kind,
                                 const std::vector<std::string>& names)
{
    set_buttons("Cancel", session_proceed_label(kind, true));
    set_lines(session_title(kind), session_waiting(names),
              "They may ask you to save your work.");
}

void SessionDialog::show_failure(SessionKind kind, const std::string& command,
                                 const std::string& reason)
{
    // the dialog maps all its buttons, so the one that is not needed goes after
    set_lines(session_title(kind) + " failed", "\"" + command + "\" " + reason,
              "Check [commands] in the configuration file.");
    set_buttons("Close", "");
}

} // namespace tiny::wm
