#include "xft_window.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cstring>

#include "style.h"
#include "x_util.h"

namespace tiny {

namespace {

XRectangle x_rectangle(const Rect& r)
{
    return {static_cast<short>(std::clamp(r.x, -32768, 32767)),
            static_cast<short>(std::clamp(r.y, -32768, 32767)),
            static_cast<unsigned short>(std::clamp(r.width, 0, 65535)),
            static_cast<unsigned short>(std::clamp(r.height, 0, 65535))};
}

} // namespace

XftWindow::XftWindow(Display& display_arg):
    atoms(display_arg),
    display(display_arg),
    screen(DefaultScreen(static_cast<::Display*>(display_arg)))
{}

XftWindow::~XftWindow()
{
    close_fonts();
    Visual* visual = DefaultVisual(display, screen);
    const Colormap colormap = DefaultColormap(display, screen);
    for (const auto& color : colors) {
        XftColorFree(display, visual, colormap, color.get());
    }
    if (draw != nullptr) {
        XftDrawDestroy(draw);
    }
    if (buffer != None) {
        XFreePixmap(display, buffer);
    }
    input.reset(); // before the window it belongs to
    if (window != None) {
        XDestroyWindow(display, window);
    }
}

void XftWindow::create(const Spec& spec)
{
    width = spec.width;
    height = spec.height;
    window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0,
                                 x_dim(width), x_dim(height), 0,
                                 BlackPixel(display, screen),
                                 WhitePixel(display, screen));
    // the whole window is painted from the buffer, so X must not clear it
    XSetWindowBackgroundPixmap(display, window, None);
    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | ButtonPressMask |
                         ButtonReleaseMask | StructureNotifyMask |
                         FocusChangeMask | spec.events);

    XStoreName(display, window, spec.title);
    XChangeProperty(display, window, atoms._NET_WM_NAME, atoms.UTF8_STRING, 8,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(spec.title),
                    static_cast<int>(std::strlen(spec.title)));
    XClassHint class_hint = {const_cast<char*>(spec.res_name),
                             const_cast<char*>(spec.res_class)};
    XSetClassHint(display, window, &class_hint);
    XSizeHints hints = {};
    hints.flags = PMinSize;
    hints.min_width = spec.min_width;
    hints.min_height = spec.min_height;
    XSetWMNormalHints(display, window, &hints);
    delete_window = atoms.WM_DELETE_WINDOW;
    XSetWMProtocols(display, window, &delete_window, 1);

    if (spec.input_method) {
        input = std::make_unique<InputMethod>(display, window, true);
    }

    new_buffer();
    draw = XftDrawCreate(display, buffer, DefaultVisual(display, screen),
                         DefaultColormap(display, screen));
}

void XftWindow::new_buffer()
{
    const Pixmap old = buffer;
    buffer =
            XCreatePixmap(display, window, x_dim(width), x_dim(height),
                          static_cast<unsigned>(DefaultDepth(display, screen)));
    if (draw != nullptr) {
        XftDrawChange(draw, buffer);
    }
    if (old != None) {
        XFreePixmap(display, old);
    }
}

void XftWindow::run()
{
    paint(); // the first Expose copies what is in the buffer
    XMapWindow(display, window);
    while (running) {
        XEvent e;
        XNextEvent(display, &e);
        dispatch(e);
    }
}

void XftWindow::dispatch(XEvent& e)
{
    if (XFilterEvent(&e, None) != 0) {
        return; // the input method used the event
    }
    switch (e.type) {
        case Expose:
            if (e.xexpose.count == 0) {
                present();
            }
            break;
        case ConfigureNotify:
            on_configure(e.xconfigure);
            break;
        case KeyPress:
            on_key(e.xkey);
            break;
        case FocusIn:
        case FocusOut:
            if (input) {
                input->set_focus(e.type == FocusIn);
            }
            break;
        case ButtonPress:
            on_button_press(e.xbutton);
            break;
        case ButtonRelease:
            on_button_release(e.xbutton);
            break;
        case ClientMessage:
            if (static_cast<Atom>(e.xclient.data.l[0]) == delete_window) {
                running = false;
            }
            break;
        default:
            break;
    }
}

void XftWindow::on_configure(const XConfigureEvent& e)
{
    if (e.width == width && e.height == height) {
        return; // only moved
    }
    width = e.width;
    height = e.height;
    on_resize(width, height);
    new_buffer();
    update();
}

void XftWindow::update()
{
    paint();
    present();
}

void XftWindow::present()
{
    XCopyArea(display, buffer, window, DefaultGC(display, screen), 0, 0,
              x_dim(width), x_dim(height), 0, 0);
}

XftColor& XftWindow::alloc_color(unsigned red, unsigned green, unsigned blue)
{
    const XRenderColor value = {static_cast<unsigned short>(red * 0x101),
                                static_cast<unsigned short>(green * 0x101),
                                static_cast<unsigned short>(blue * 0x101),
                                0xffff};
    XftColor& color = *colors.emplace_back(std::make_unique<XftColor>());
    XftColorAllocValue(display, DefaultVisual(display, screen),
                       DefaultColormap(display, screen), &value, &color);
    return color;
}

XftFont* XftWindow::font(const std::string& family, int pixel_size)
{
    pixel_size = std::max(1, pixel_size); // 0 is the key of the names
    const auto key = std::make_pair(family, pixel_size);
    const auto found = fonts.find(key);
    if (found != fonts.end()) {
        return found->second;
    }
    // by the name: the family may be a list ("DejaVu Sans,sans")
    XftFont* opened = XftFontOpenName(
            display, screen, xft_pixel_name(family, pixel_size).c_str());
    fonts[key] = opened;
    return opened;
}

XftFont* XftWindow::font_by_name(const std::string& xft_name)
{
    const auto key = std::make_pair(xft_name, 0);
    const auto found = fonts.find(key);
    if (found != fonts.end()) {
        return found->second;
    }
    XftFont* opened = XftFontOpenName(display, screen, xft_name.c_str());
    fonts[key] = opened;
    return opened;
}

void XftWindow::close_fonts()
{
    for (const auto& entry : fonts) {
        if (entry.second != nullptr) {
            XftFontClose(display, entry.second);
        }
    }
    fonts.clear();
}

int XftWindow::text_width(XftFont* f, const std::string& text) const
{
    if (f == nullptr) {
        return 0;
    }
    XGlyphInfo info;
    XftTextExtentsUtf8(display, f,
                       reinterpret_cast<const FcChar8*>(text.c_str()),
                       static_cast<int>(text.size()), &info);
    return info.xOff;
}

std::string XftWindow::fit(XftFont* f, const std::string& text,
                           int max_width) const
{
    if (text_width(f, text) <= max_width) {
        return text;
    }
    constexpr const char* ellipsis = "\xE2\x80\xA6";
    std::string cut = text;
    while (!cut.empty()) {
        const auto last = static_cast<unsigned char>(cut.back());
        cut.pop_back();
        // a character is gone when its first byte is (not a continuation)
        if ((last & 0xC0) != 0x80 &&
            text_width(f, cut + ellipsis) <= max_width) {
            return cut + ellipsis;
        }
    }
    return ellipsis;
}

std::vector<std::string> XftWindow::wrap(XftFont* f, const std::string& text,
                                         int max_width) const
{
    std::vector<std::string> lines(1);
    const auto add_word = [&](const std::string& word) {
        std::string& line = lines.back();
        const std::string joined = line.empty() ? word : line + " " + word;
        if (line.empty() || text_width(f, joined) <= max_width) {
            line = joined;
        } else {
            lines.push_back(word);
        }
        // a word wider than the line is cut by characters
        while (lines.back().size() > 1 &&
               text_width(f, lines.back()) > max_width) {
            std::string& last = lines.back();
            size_t cut = last.size() - 1;
            while (cut > 1 &&
                   (text_width(f, last.substr(0, cut)) > max_width ||
                    (static_cast<unsigned char>(last[cut]) & 0xC0) == 0x80)) {
                --cut;
            }
            // at least one whole character stays on the line
            while (cut < last.size() &&
                   (static_cast<unsigned char>(last[cut]) & 0xC0) == 0x80) {
                ++cut;
            }
            if (cut >= last.size()) {
                break;
            }
            const std::string rest = last.substr(cut);
            last.resize(cut);
            lines.push_back(rest);
        }
    };

    std::string word;
    for (const char c : text) {
        if (c == '\n') {
            if (!word.empty()) {
                add_word(word);
                word.clear();
            }
            lines.emplace_back();
        } else if (c == ' ') {
            if (!word.empty()) {
                add_word(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        add_word(word);
    }
    return lines;
}

void XftWindow::fill(const XftColor& color, const Rect& r)
{
    XftDrawRect(draw, &color, r.x, r.y, x_uint(r.width), x_uint(r.height));
}

void XftWindow::frame(const XftColor& color, const Rect& r, int line)
{
    fill(color, {r.x, r.y, r.width, line});
    fill(color, {r.x, r.y + r.height - line, r.width, line});
    fill(color, {r.x, r.y, line, r.height});
    fill(color, {r.x + r.width - line, r.y, line, r.height});
}

void XftWindow::clip(const Rect& r)
{
    const XRectangle area = x_rectangle(r);
    XftDrawSetClipRectangles(draw, 0, 0, &area, 1);
}

void XftWindow::unclip()
{
    XftDrawSetClip(draw, nullptr);
}

void XftWindow::text_at(XftFont* f, const XftColor& color, int x, int baseline,
                        const std::string& text)
{
    XftDrawStringUtf8(draw, &color, f, x, baseline,
                      reinterpret_cast<const FcChar8*>(text.c_str()),
                      static_cast<int>(text.size()));
}

void XftWindow::text_in(XftFont* f, const XftColor& color, const Rect& r,
                        int left, const std::string& text, int shift)
{
    if (f == nullptr) {
        return;
    }
    clip(r);
    text_at(f, color, r.x + left + shift,
            r.y + (r.height + f->ascent - f->descent) / 2, text);
    unclip();
}

std::string XftWindow::lookup(XKeyEvent& e, KeySym& symbol) const
{
    if (input) {
        return input->lookup(e, symbol);
    }
    symbol = XLookupKeysym(&e, 0);
    return {};
}

} // namespace tiny
