// tiny-settings: changes the font, the terminal and the scale of tiny-shell.
//
// The form has the fields of the font, its size and style, the terminal and
// the scale; a field has a cursor (the arrows, Home, End, Delete and the
// pointer move it). The button next to the font opens a list of the fonts of
// the system (type to search it) with the size and the style of the font and
// a preview of a text in them. Save writes the three values to the
// configuration file of the user (never the global one) and closes the
// program; tiny-shell reads the file when it starts, so a change is seen after
// the next login. Escape and Cancel close the program without writing.
//
// The text is drawn by Xft into a pixmap that is copied to the window, so the
// window does not flicker. What can be decided without X (the fields, the
// list, the keys) is in SettingsForm.

#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "display.h"
#include "entry.h"
#include "settings_form.h"
#include "text_field.h"
#include "style.h"
#include "wm/config.h"
#include "x_util.h"
#include "xdg/font_list.h"

namespace {

using tiny::SettingsForm;

constexpr const char* title = "Settings";

//! More fonts than this are closed and opened again when they are needed
constexpr size_t font_cache_limit = 64;

//! The initial height of the window, at the scale 1
constexpr int start_height = 600;
constexpr int start_width = 560;

constexpr const char* hint = "Takes effect after you log in again.";

XRenderColor render_color(unsigned red, unsigned green, unsigned blue)
{
    return {static_cast<unsigned short>(red * 0x101),
            static_cast<unsigned short>(green * 0x101),
            static_cast<unsigned short>(blue * 0x101), 0xffff};
}

XRectangle x_rectangle(int x, int y, int width, int height)
{
    return {static_cast<short>(std::clamp(x, -32768, 32767)),
            static_cast<short>(std::clamp(y, -32768, 32767)),
            static_cast<unsigned short>(std::clamp(width, 0, 65535)),
            static_cast<unsigned short>(std::clamp(height, 0, 65535))};
}

//! The scale as it is written in the file: 1.5, not 1.500000
std::string number_text(double value)
{
    char text[32];
    snprintf(text, sizeof text, "%g", value);
    return text;
}

class SettingsApp {
  public:
    SettingsApp(tiny::Display& display_arg, const tiny::wm::Config& config,
                std::string file_arg, std::vector<std::string> families):
        atoms(display_arg),
        display(display_arg),
        screen(DefaultScreen(static_cast<::Display*>(display_arg))),
        file(std::move(file_arg)),
        scale(config.scale),
        metrics(SettingsForm::Metrics::scaled(config.scale)),
        initial{config.font, config.terminal_line, number_text(config.scale),
                number_text(config.font_size), config.font_style},
        form(initial, std::move(families), metrics, tiny::xdg::font_styles)
    {
        Visual* visual = DefaultVisual(display, screen);
        const Colormap colormap = DefaultColormap(display, screen);
        alloc_color(background, render_color(0xf8, 0xf8, 0xf8));
        alloc_color(field, render_color(0xff, 0xff, 0xff));
        alloc_color(text_color, render_color(0x2e, 0x34, 0x36));
        alloc_color(dim, render_color(0x6e, 0x74, 0x76));
        alloc_color(rule, render_color(0xc8, 0xc8, 0xc8));
        alloc_color(accent, render_color(0x72, 0x9f, 0xcf));
        alloc_color(selected, render_color(0xdc, 0xe8, 0xf5));
        alloc_color(error, render_color(0xc0, 0x1c, 0x28));

        // the font of the shell, when it can be opened, else any sans
        ui_family = config.font;
        ui_font = font(ui_family);
        if (ui_font == nullptr) {
            ui_family = "sans";
            ui_font = font(ui_family);
        }

        form.set_measure([this](const std::string& text) {
            return ui_font != nullptr ? text_width(ui_font, text) : 0;
        });

        create_window();
        draw = XftDrawCreate(display, buffer, visual, colormap);
    }

    SettingsApp(const SettingsApp&) = delete;
    SettingsApp& operator=(const SettingsApp&) = delete;

    ~SettingsApp()
    {
        close_fonts();
        XftDrawDestroy(draw);
        XFreePixmap(display, buffer);
        Visual* visual = DefaultVisual(display, screen);
        const Colormap colormap = DefaultColormap(display, screen);
        for (XftColor* color : {&background, &field, &text_color, &dim, &rule,
                                &accent, &selected, &error}) {
            XftColorFree(display, visual, colormap, color);
        }
        if (xic != nullptr) {
            XDestroyIC(xic);
        }
        if (xim != nullptr) {
            XCloseIM(xim);
        }
        XDestroyWindow(display, window);
    }

    void run()
    {
        paint(); // the first Expose copies what is in the buffer
        XMapWindow(display, window);
        while (running) {
            XEvent e;
            XNextEvent(display, &e);
            if (XFilterEvent(&e, None) != 0) {
                continue; // the input method used the event
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
                    if (xic != nullptr) {
                        XSetICFocus(xic);
                    }
                    break;
                case FocusOut:
                    if (xic != nullptr) {
                        XUnsetICFocus(xic);
                    }
                    break;
                case ButtonPress:
                    on_button_press(e.xbutton);
                    break;
                case ButtonRelease:
                    if (e.xbutton.button == Button1 &&
                        form.release(e.xbutton.x, e.xbutton.y)) {
                        update();
                    }
                    break;
                case ClientMessage:
                    if (static_cast<Atom>(e.xclient.data.l[0]) ==
                        delete_window) {
                        running = false;
                    }
                    break;
                default:
                    break;
            }
        }
    }

  private:
    void alloc_color(XftColor& target, const XRenderColor& color)
    {
        XftColorAllocValue(display, DefaultVisual(display, screen),
                           DefaultColormap(display, screen), &color, &target);
    }

    void create_window()
    {
        width = std::min(tiny::scaled(start_width, scale),
                         DisplayWidth(display, screen));
        height = std::min(
                std::max(tiny::scaled(start_height, scale), form.form_height()),
                DisplayHeight(display, screen));
        form.resize(width, height);

        window = XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0,
                                     tiny::x_dim(width), tiny::x_dim(height), 0,
                                     BlackPixel(display, screen),
                                     WhitePixel(display, screen));
        // the whole window is painted from the buffer, so X must not clear it
        XSetWindowBackgroundPixmap(display, window, None);
        XSelectInput(display, window,
                     ExposureMask | KeyPressMask | ButtonPressMask |
                             ButtonReleaseMask | StructureNotifyMask |
                             FocusChangeMask);

        XStoreName(display, window, title);
        XChangeProperty(display, window, atoms._NET_WM_NAME, atoms.UTF8_STRING,
                        8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(title),
                        static_cast<int>(std::strlen(title)));
        XClassHint class_hint = {const_cast<char*>("tiny-settings"),
                                 const_cast<char*>("TinySettings")};
        XSetClassHint(display, window, &class_hint);
        XSizeHints hints = {};
        hints.flags = PMinSize;
        hints.min_width = tiny::scaled(start_width, scale) / 2;
        hints.min_height = form.form_height();
        XSetWMNormalHints(display, window, &hints);
        delete_window = atoms.WM_DELETE_WINDOW;
        XSetWMProtocols(display, window, &delete_window, 1);

        // an input method gives the characters that are not Latin-1; without
        // one the keys give Latin-1 only
        if (XSupportsLocale() != False) {
            XSetLocaleModifiers("");
            xim = XOpenIM(display, nullptr, nullptr, nullptr);
        }
        if (xim != nullptr) {
            xic = XCreateIC(
                    xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                    XNClientWindow, window, XNFocusWindow, window, nullptr);
        }

        new_buffer();
    }

    void new_buffer()
    {
        const Pixmap old = buffer;
        buffer = XCreatePixmap(
                display, window, tiny::x_dim(width), tiny::x_dim(height),
                static_cast<unsigned>(DefaultDepth(display, screen)));
        if (draw != nullptr) {
            XftDrawChange(draw, buffer);
        }
        if (old != None) {
            XFreePixmap(display, old);
        }
    }

    void on_configure(const XConfigureEvent& e)
    {
        if (e.width == width && e.height == height) {
            return; // only moved
        }
        width = e.width;
        height = e.height;
        form.resize(width, height);
        new_buffer();
        update();
    }

    //! The characters of a key, in UTF-8, and its symbol
    std::string lookup(XKeyEvent& e, KeySym& symbol)
    {
        if (xic == nullptr) { // Latin-1 only
            char buf[32];
            const int len =
                    XLookupString(&e, buf, sizeof buf, &symbol, nullptr);
            return tiny::latin1_to_utf8(buf, len);
        }
        std::string buf(32, '\0');
        Status state = 0;
        int len = Xutf8LookupString(xic, &e, buf.data(),
                                    static_cast<int>(buf.size()), &symbol,
                                    &state);
        if (state == XBufferOverflow) { // `len` is the needed size
            buf.assign(static_cast<size_t>(len), '\0');
            len = Xutf8LookupString(xic, &e, buf.data(),
                                    static_cast<int>(buf.size()), &symbol,
                                    &state);
        }
        if (state != XLookupChars && state != XLookupBoth) {
            return {}; // a key without characters; `len` means nothing
        }
        buf.resize(static_cast<size_t>(std::max(len, 0)));
        return buf;
    }

    void on_key(XKeyEvent& e)
    {
        KeySym symbol = NoSymbol;
        const std::string typed = lookup(e, symbol);
        if ((e.state & ControlMask) != 0) {
            if (symbol == XK_q || symbol == XK_w) {
                running = false;
            }
            return;
        }
        static const std::pair<KeySym, SettingsForm::Key> keys[] = {
                {XK_Up, SettingsForm::Key::Up},
                {XK_Down, SettingsForm::Key::Down},
                {XK_Left, SettingsForm::Key::Left},
                {XK_Right, SettingsForm::Key::Right},
                {XK_Delete, SettingsForm::Key::Delete},
                {XK_KP_Delete, SettingsForm::Key::Delete},
                {XK_Page_Up, SettingsForm::Key::PageUp},
                {XK_Page_Down, SettingsForm::Key::PageDown},
                {XK_Home, SettingsForm::Key::Home},
                {XK_End, SettingsForm::Key::End},
                {XK_Return, SettingsForm::Key::Return},
                {XK_KP_Enter, SettingsForm::Key::Return},
                {XK_BackSpace, SettingsForm::Key::Back},
                {XK_Escape, SettingsForm::Key::Escape},
                {XK_Tab, SettingsForm::Key::Tab},
                {XK_ISO_Left_Tab, SettingsForm::Key::BackTab}};
        for (const auto& [code, key] : keys) {
            if (symbol == code) {
                const bool back =
                        key == SettingsForm::Key::Tab && (e.state & ShiftMask);
                if (form.key(back ? SettingsForm::Key::BackTab : key)) {
                    update();
                }
                return;
            }
        }
        if (form.type(typed)) {
            update();
        }
    }

    void on_button_press(const XButtonEvent& e)
    {
        if (e.button == Button1) {
            form.press(e.x, e.y);
        } else if ((e.button == Button4 || e.button == Button5) &&
                   form.wheel(e.button == Button4 ? -1 : 1)) {
            update();
        }
    }

    //! Paint after something changed, and act on what the user asked for
    void update()
    {
        switch (form.result()) {
            case SettingsForm::Result::Editing:
                break;
            case SettingsForm::Result::Cancelled:
                running = false;
                return;
            case SettingsForm::Result::Saved:
                if (save()) {
                    running = false;
                    return;
                }
                break;
        }
        paint();
        present();
    }

    //! Write the file of the user; on a failure the form tells why
    bool save()
    {
        std::string why;
        if (file.empty()) {
            why = "Neither XDG_CONFIG_HOME nor HOME is set.";
        } else {
            // only what the user changed: the values that come from the global
            // file stay there, so a later change of that file is still seen
            const SettingsForm::Values v = form.clean_values();
            std::vector<tiny::wm::IniValue> changed;
            if (v.font != initial.font) {
                changed.push_back({"theme", "font", v.font});
            }
            if (v.scale != initial.scale) {
                changed.push_back({"theme", "scale", v.scale});
            }
            if (v.size != initial.size) {
                changed.push_back({"theme", "font_size", v.size});
            }
            if (v.style != initial.style) {
                changed.push_back({"theme", "font_style", v.style});
            }
            if (v.terminal != initial.terminal) {
                changed.push_back({"commands", "terminal", v.terminal});
            }
            if (changed.empty() || tiny::wm::save_values(file, changed, &why)) {
                return true;
            }
        }
        form.reject("Not saved: " + why);
        return false;
    }

    void present()
    {
        XCopyArea(display, buffer, window, DefaultGC(display, screen), 0, 0,
                  tiny::x_dim(width), tiny::x_dim(height), 0, 0);
    }

    // A font of a family at the size of the text; null when it can't be opened
    XftFont* font(const std::string& family)
    {
        const auto found = fonts.find(family);
        if (found != fonts.end()) {
            return found->second;
        }
        XftFont* opened =
                XftFontOpen(display, screen, FC_FAMILY, XftTypeString,
                            family.c_str(), FC_PIXEL_SIZE, XftTypeDouble,
                            static_cast<double>(metrics.text_size), nullptr);
        fonts[family] = opened;
        return opened;
    }

    void close_fonts()
    {
        for (const auto& entry : fonts) {
            if (entry.second != nullptr) {
                XftFontClose(display, entry.second);
            }
        }
        fonts.clear();
    }

    int text_width(XftFont* f, const std::string& text) const
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

    void clip(const tiny::Rect& r)
    {
        const XRectangle area = x_rectangle(r.x, r.y, r.width, r.height);
        XftDrawSetClipRectangles(draw, 0, 0, &area, 1);
    }

    void unclip() { XftDrawSetClip(draw, nullptr); }

    void fill(const XftColor& color, const tiny::Rect& r)
    {
        XftDrawRect(draw, &color, r.x, r.y, tiny::x_uint(r.width),
                    tiny::x_uint(r.height));
    }

    //! A frame of one pixel (two at a high scale) inside of the rectangle
    void frame(const XftColor& color, const tiny::Rect& r)
    {
        const int line = std::max(1, metrics.row_height / 34);
        fill(color, {r.x, r.y, r.width, line});
        fill(color, {r.x, r.y + r.height - line, r.width, line});
        fill(color, {r.x, r.y, line, r.height});
        fill(color, {r.x + r.width - line, r.y, line, r.height});
    }

    //! The text with its baseline in the middle of the rectangle, `left` from
    //! its start and moved by `shift` (to the left, to show the cursor)
    void text_in(XftFont* f, const XftColor& color, const tiny::Rect& r,
                 int left, const std::string& text, int shift = 0)
    {
        if (f == nullptr) {
            return;
        }
        const int baseline = r.y + (r.height + f->ascent - f->descent) / 2;
        clip(r);
        XftDrawStringUtf8(draw, &color, f, r.x + left + shift, baseline,
                          reinterpret_cast<const FcChar8*>(text.c_str()),
                          static_cast<int>(text.size()));
        unclip();
    }

    //! A field with its text, and the cursor when it has the focus
    void paint_field(const tiny::Rect& r, const tiny::TextField& edited,
                     bool focused, bool wrong)
    {
        const int padding = metrics.padding;
        fill(field, r);
        const XftColor* border = focused ? &accent : &rule;
        if (wrong) {
            border = &error;
        }
        frame(*border, r);
        const int shift = form.shift(edited, r.width - 2 * padding);
        text_in(ui_font, text_color, r, padding, edited.text(), shift);
        if (focused && ui_font != nullptr) {
            const int before = text_width(
                    ui_font, edited.text().substr(0, edited.cursor()));
            const int line = std::max(1, metrics.row_height / 34);
            fill(text_color,
                 {r.x + padding + shift + before, r.y + metrics.row_height / 5,
                  line, metrics.row_height * 3 / 5});
        }
    }

    void paint()
    {
        fill(background, {0, 0, width, height});
        if (form.view() == SettingsForm::View::Picker) {
            paint_picker();
        } else {
            paint_form();
        }
    }

    void paint_button(const tiny::Rect& r, const std::string& label,
                      bool enabled)
    {
        fill(field, r);
        frame(enabled ? accent : rule, r);
        text_in(ui_font, enabled ? text_color : dim, r,
                std::max(0, (r.width - text_width(ui_font, label)) / 2), label);
    }

    //! The two rows of the message: what is wrong, then the hint
    void paint_message(const std::string& wrong, const char* note)
    {
        const tiny::Rect message = form.message_rect();
        const tiny::Rect first = {message.x, message.y, message.width,
                                  message.height / 2};
        const tiny::Rect second = {message.x, message.y + message.height / 2,
                                   message.width, message.height / 2};
        text_in(ui_font, error, first, 0, wrong);
        text_in(ui_font, dim, second, 0, note);
    }

    void paint_form()
    {
        const struct {
            SettingsForm::Field id;
            const char* label;
        } rows[] = {{SettingsForm::Field::Font, "Font"},
                    {SettingsForm::Field::Size, "Size"},
                    {SettingsForm::Field::Style, "Style"},
                    {SettingsForm::Field::Terminal, "Terminal"},
                    {SettingsForm::Field::Scale, "Scale"}};
        for (const auto& row : rows) {
            text_in(ui_font, text_color, form.label_rect(row.id), 0, row.label);
            paint_field(form.field_rect(row.id), form.field(row.id),
                        form.focus() == row.id, !form.problem(row.id).empty());
        }
        paint_button(form.choose_rect(), "Choose…", true);

        std::string wrong = form.problem();
        if (wrong.empty()) {
            wrong = form.status();
        }
        paint_message(wrong, hint);

        paint_button(form.cancel_rect(), "Cancel", true);
        paint_button(form.save_rect(), "Save", form.problem().empty());
    }

    // A font by its Xft name (family, size in points and style); null when it
    // can't be opened
    XftFont* named_font(const std::string& name)
    {
        const auto found = fonts.find(name);
        if (found != fonts.end()) {
            return found->second;
        }
        XftFont* opened = XftFontOpenName(display, screen, name.c_str());
        fonts[name] = opened;
        return opened;
    }

    void paint_preview()
    {
        const tiny::Rect r = form.preview_rect();
        fill(field, r);
        frame(rule, r);
        if (!form.pick_problem().empty()) {
            text_in(ui_font, error, r, metrics.padding, form.pick_problem());
            return;
        }
        if (form.preview_family().empty()) {
            return;
        }
        // the size is the one of the shell, so it follows the scale
        XftFont* f = named_font(tiny::xft_name(
                form.preview_family(), form.preview_size() * scale,
                tiny::xft_style(form.pick_style())));
        text_in(f != nullptr ? f : ui_font, text_color, r, metrics.padding,
                SettingsForm::preview_text());
    }

    void paint_picker()
    {
        // the fonts of the rows that show are never thrown away mid-paint
        if (fonts.size() > font_cache_limit + 2 * form.visible_rows()) {
            close_fonts();
            ui_font = font(ui_family);
        }
        const int padding = metrics.padding;
        const tiny::Rect search = form.search_rect();
        const bool in_search = form.pane() == SettingsForm::Pane::Search;
        if (form.query().empty() && !in_search) {
            fill(field, search);
            frame(rule, search);
            text_in(ui_font, dim, search, padding, "Type to search the fonts");
        } else {
            paint_field(search, form.search_field(), in_search, false);
        }

        if (form.families().empty()) {
            const tiny::Rect none = form.row_rect(0);
            text_in(ui_font, dim, none, padding, "No font matches");
        }
        const size_t end = std::min(form.families().size(),
                                    form.first() + form.visible_rows());
        for (size_t i = form.first(); i < end; ++i) {
            const tiny::Rect row = form.row_rect(i);
            const std::string& family = form.families()[i];
            if (i == form.selected()) {
                fill(selected, row);
            }
            // every family in its own font; the name in the font of the form
            // when the family has no glyphs for it
            XftFont* f = font(family);
            text_in(f != nullptr ? f : ui_font, text_color, row, metrics.margin,
                    family);
        }

        paint_preview();

        text_in(ui_font, text_color, form.size_label_rect(), 0, "Size");
        paint_field(form.size_rect(), form.size_field(),
                    form.pane() == SettingsForm::Pane::Size,
                    !form.pick_problem().empty());
        text_in(ui_font, text_color, form.style_label_rect(), 0, "Style");
        const tiny::Rect style = form.style_rect();
        fill(field, style);
        frame(form.pane() == SettingsForm::Pane::Style ? accent : rule, style);
        const std::string label =
                form.pick_style().empty() ? "Default" : form.pick_style();
        text_in(ui_font, text_color, style, padding,
                "\xE2\x80\xB9  " + label + "  \xE2\x80\xBA");

        paint_button(form.cancel_rect(), "Cancel", true);
        paint_button(form.save_rect(), "Select", form.pick_problem().empty());
    }

    tiny::Display& atoms;
    ::Display* display;
    int screen;
    std::string file; //!< of the user, empty when its place is not known
    double scale;
    SettingsForm::Metrics metrics;
    SettingsForm::Values initial; //!< what the files say now
    SettingsForm form;
    ::Window window = None;
    Pixmap buffer = None;
    Atom delete_window = None;
    XIM xim = nullptr;
    XIC xic = nullptr;
    XftDraw* draw = nullptr;
    XftFont* ui_font = nullptr;
    std::string ui_family; //!< the family of `ui_font`
    XftColor background{};
    XftColor field{};
    XftColor text_color{};
    XftColor dim{};
    XftColor rule{};
    XftColor accent{};
    XftColor selected{};
    XftColor error{};
    std::map<std::string, XftFont*> fonts;
    int width = 0;
    int height = 0;
    bool running = true;
};

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1) {
        const bool help = std::strcmp(argv[1], "--help") == 0;
        fprintf(help ? stdout : stderr,
                "usage: %s\nChanges the font (its size and style), the "
                "terminal "
                "and the scale of tiny-shell.\nThe settings are written to the "
                "configuration "
                "file of the user and are used\nafter the next login.\n",
                argv[0]);
        return help ? EXIT_SUCCESS : 2;
    }
    // the characters only: the numbers of the file always have a decimal point
    setlocale(LC_CTYPE, "");

    // the values in use now: the files of the system and of the user together
    std::vector<std::string> problems;
    const tiny::wm::Config config = tiny::wm::load_config(
            tiny::wm::config_files(getenv("XDG_CONFIG_HOME"),
                                   getenv("XDG_CONFIG_DIRS"), getenv("HOME")),
            &problems);
    for (const std::string& problem : problems) {
        fprintf(stderr, "%s: %s\n", argv[0], problem.c_str());
    }

    try {
        tiny::Display::init();
        SettingsApp app(tiny::get_display(), config,
                        tiny::wm::user_config_file(getenv("XDG_CONFIG_HOME"),
                                                   getenv("HOME")),
                        tiny::xdg::font_families());
        app.run();
    } catch (const std::exception& e) {
        fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
