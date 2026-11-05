#include "clipboard.h"

#include <X11/Xatom.h>

#include <vector>

#include "display.h"

namespace tiny {

namespace {

constexpr long max_paste = 1 << 16; //!< 32-bit words asked of the property

//! The Latin-1 text for the old `STRING` target; what it cannot have is "?"
std::string to_latin1(const std::string& utf8)
{
    std::string out;
    for (size_t i = 0; i < utf8.size();) {
        const auto c = static_cast<unsigned char>(utf8[i]);
        unsigned code = c;
        size_t length = 1;
        if (c >= 0xF0) {
            length = 4;
        } else if (c >= 0xE0) {
            length = 3;
        } else if (c >= 0xC0) {
            length = 2;
        }
        if (length == 2 && i + 1 < utf8.size()) {
            code = ((c & 0x1Fu) << 6) |
                   (static_cast<unsigned char>(utf8[i + 1]) & 0x3Fu);
        } else if (length > 2) {
            code = 0x100;
        }
        out += code < 0x100 ? static_cast<char>(code) : '?';
        i += length;
    }
    return out;
}

} // namespace

std::string single_line(const std::string& text)
{
    std::string out;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (c == '\n' || c == '\r' || c == '\t') {
            out += ' ';
        } else if (byte >= 0x20 && byte != 0x7F) {
            out += c;
        }
    }
    return out;
}

Clipboard::Clipboard(::Display* display_arg, ::Window window_arg):
    display(display_arg), window(window_arg)
{
    primary.selection = XA_PRIMARY;
    board.selection = get_display().CLIPBOARD;
}

Clipboard::~Clipboard()
{
    for (Slot* s : {&primary, &board}) {
        if (s->ours && XGetSelectionOwner(display, s->selection) == window) {
            XSetSelectionOwner(display, s->selection, None, CurrentTime);
        }
    }
}

void Clipboard::request(Which which, Receive receive, Time time)
{
    Slot& s = slot(which);
    s.receive = std::move(receive);
    if (s.ours) { // our own text needs no round trip
        const auto done = std::move(s.receive);
        s.receive = nullptr;
        if (!s.text.empty()) {
            done(s.text);
        }
        return;
    }
    XConvertSelection(display, s.selection, get_display().UTF8_STRING,
                      get_display().TINY_SELECTION, window, time);
}

void Clipboard::own(Which which, std::string text, Time time)
{
    Slot& s = slot(which);
    XSetSelectionOwner(display, s.selection, window, time);
    s.ours = XGetSelectionOwner(display, s.selection) == window;
    s.text = s.ours ? std::move(text) : std::string();
}

bool Clipboard::owns(Which which) const
{
    return slot(which).ours;
}

const std::string& Clipboard::owned(Which which) const
{
    return slot(which).text;
}

Clipboard::Slot* Clipboard::slot_of(Atom selection)
{
    if (selection == primary.selection) {
        return &primary;
    }
    return selection == board.selection ? &board : nullptr;
}

bool Clipboard::handle(const XEvent& e)
{
    switch (e.type) {
        case SelectionNotify:
            on_notify(e.xselection);
            return true;
        case SelectionRequest:
            on_request(e.xselectionrequest);
            return true;
        case SelectionClear:
            if (Slot* s = slot_of(e.xselectionclear.selection)) {
                s->ours = false;
                s->text.clear();
            }
            return true;
        default:
            return false;
    }
}

void Clipboard::on_notify(const XSelectionEvent& e)
{
    Slot* s = slot_of(e.selection);
    if (s == nullptr || !s->receive) {
        return;
    }
    const auto done = std::move(s->receive);
    s->receive = nullptr;
    if (e.property == None) { // the owner has no text of this kind
        return;
    }

    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long after = 0;
    unsigned char* data = nullptr;
    const int status = XGetWindowProperty(
            display, window, e.property, 0, max_paste, True, AnyPropertyType,
            &type, &format, &count, &after, &data);
    std::string text;
    if (status == Success && data != nullptr && format == 8 &&
        type != get_display().INCR) {
        text.assign(reinterpret_cast<const char*>(data), count);
    }
    if (data != nullptr) {
        XFree(data);
    }
    if (!text.empty()) {
        done(text);
    }
}

void Clipboard::on_request(const XSelectionRequestEvent& e)
{
    const Display& atoms = get_display();
    const Slot* s = slot_of(e.selection);
    // an old client sends no property: the target is the property then
    const Atom property = e.property != None ? e.property : e.target;

    XEvent reply = {};
    reply.xselection.type = SelectionNotify;
    reply.xselection.requestor = e.requestor;
    reply.xselection.selection = e.selection;
    reply.xselection.target = e.target;
    reply.xselection.property = None;
    reply.xselection.time = e.time;

    if (s != nullptr && s->ours) {
        if (e.target == atoms.TARGETS) {
            const std::vector<Atom> targets = {atoms.TARGETS, atoms.UTF8_STRING,
                                               XA_STRING};
            XChangeProperty(
                    display, e.requestor, property, XA_ATOM, 32,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(targets.data()),
                    static_cast<int>(targets.size()));
            reply.xselection.property = property;
        } else if (e.target == atoms.UTF8_STRING || e.target == XA_STRING) {
            const std::string data =
                    e.target == XA_STRING ? to_latin1(s->text) : s->text;
            XChangeProperty(display, e.requestor, property, e.target, 8,
                            PropModeReplace,
                            reinterpret_cast<const unsigned char*>(data.data()),
                            static_cast<int>(data.size()));
            reply.xselection.property = property;
        }
    }
    XSendEvent(display, e.requestor, False, NoEventMask, &reply);
    XFlush(display);
}

} // namespace tiny
