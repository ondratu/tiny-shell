#include "input_method.h"

#include <algorithm>
#include <array>

namespace tiny {

std::string latin1_to_utf8(const char* bytes, int len)
{
    std::string out;
    for (int i = 0; i < len; ++i) {
        const auto c = static_cast<unsigned char>(bytes[i]);
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    return out;
}

InputMethod::InputMethod(::Display* display, ::Window window, bool environment)
{
    if (environment) {
        if (XSupportsLocale() != False) {
            XSetLocaleModifiers(""); // XMODIFIERS: ibus, fcitx, ...
            open(display, window);
        }
        if (xic == nullptr) {
            // The method of the environment is not there (a nested X server
            // has no ibus of the session) or the locale is not supported: the
            // built-in method of Xlib still types every character, not only
            // Latin-1.
            XSetLocaleModifiers("@im=none");
            open(display, window);
        }
        return;
    }
    open(display, window);
}

void InputMethod::open(::Display* display, ::Window window)
{
    xim = XOpenIM(display, nullptr, nullptr, nullptr);
    if (xim == nullptr) {
        return;
    }
    xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, window, XNFocusWindow, window, nullptr);
    if (xic == nullptr) { // a method that can't do the style is no use
        XCloseIM(xim);
        xim = nullptr;
    }
}

InputMethod::~InputMethod()
{
    if (xic != nullptr) {
        XDestroyIC(xic);
    }
    if (xim != nullptr) {
        XCloseIM(xim);
    }
}

std::string InputMethod::lookup(const XKeyEvent& key, KeySym& keysym) const
{
    // the Xlib functions take a non-const event but do not change it
    auto* const event = const_cast<XKeyEvent*>(&key);
    if (xic == nullptr) { // Latin-1 only
        std::array<char, 32> buf{};
        const int len =
                XLookupString(event, buf.data(), static_cast<int>(buf.size()),
                              &keysym, nullptr);
        return latin1_to_utf8(buf.data(), len);
    }

    std::string buf(32, '\0');
    Status state = 0;
    int len = Xutf8LookupString(xic, event, buf.data(),
                                static_cast<int>(buf.size()), &keysym, &state);
    if (state == XBufferOverflow) { // `len` is the needed size
        buf.assign(static_cast<size_t>(len), '\0');
        len = Xutf8LookupString(xic, event, buf.data(),
                                static_cast<int>(buf.size()), &keysym, &state);
    }
    if (state != XLookupChars && state != XLookupBoth) {
        return {}; // a key without characters; `len` means nothing
    }
    buf.resize(static_cast<size_t>(std::max(len, 0)));
    return buf;
}

void InputMethod::set_focus(bool focus) const
{
    if (xic != nullptr) {
        focus ? XSetICFocus(xic) : XUnsetICFocus(xic);
    }
}

} // namespace tiny
