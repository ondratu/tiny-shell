// tiny-fonts: lists the fonts of the system and shows a specimen of one.
//
// The overview is a grid with "Aa" in every font family and its name under it.
// A click (or Return) on a family opens its specimen: the letters, the digits
// and the signs, then a sentence in growing sizes. The button Info (or I) at
// the top right shows what is known about the family: the files, the
// languages, the designer and the license. Escape or backspace go one step
// back, the breadcrumbs at the top ("‹ Fonts ‹ Family") go to the overview or
// to the specimen. Ctrl+Q and Ctrl+W quit.
//
// The list comes from fontconfig, the text is drawn by Xft into a pixmap
// that is copied to the window, so the window does not flicker. What can be
// decided without X (the grid, the scrolling, the keys) is in FontBrowser.

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "clipboard.h"
#include "display.h"
#include "app/font_browser.h"
#include "style.h"
#include "wm/config.h"
#include "x_util.h"
#include "xdg/font_info.h"
#include "xdg/font_list.h"
#include "xft_window.h"

namespace {

using tiny::app::FontBrowser;

constexpr const char* title = "Fonts";

//! More fonts than this are closed and opened again when they are needed
constexpr size_t font_cache_limit = 128;

//! The cells that are visible at the start, and the share of the screen
constexpr int start_columns = 4;
constexpr int start_rows = 3;

class FontsApp : public tiny::XftWindow {
  public:
    FontsApp(tiny::Display& display_arg, std::vector<std::string> families,
             double scale):
        XftWindow(display_arg),
        metrics(FontBrowser::Metrics::scaled(scale)),
        browser(std::move(families), metrics),
        black(alloc_gray(0)),
        white(alloc_gray(255)),
        selected(alloc_gray(0xe8)),
        rule(alloc_gray(0xc8)),
        dim(alloc_gray(0x6e))
    {
        // not in the cache of fonts: that is emptied while scrolling
        ui_font = XftFontOpen(display, screen, FC_FAMILY, XftTypeString, "sans",
                              FC_PIXEL_SIZE, XftTypeDouble,
                              static_cast<double>(metrics.name_size), nullptr);

        const int margin = metrics.margin;
        const int w = std::min(start_columns * metrics.cell_width + 2 * margin,
                               DisplayWidth(display, screen));
        const int h = std::min(start_rows * metrics.cell_height + 2 * margin,
                               DisplayHeight(display, screen) * 3 / 4);
        browser.resize(w, h);
        create({title, "tiny-fonts", "TinyFonts", w, h,
                metrics.cell_width + 2 * margin, metrics.cell_height, 0, true});
    }

    ~FontsApp() override
    {
        if (ui_font != nullptr) {
            XftFontClose(display, ui_font);
        }
    }

  protected:
    void on_resize(int w, int h) override
    {
        browser.resize(w, h);
        info_measured.clear(); // wrapped to the old width
    }

    void on_button_release(const XButtonEvent& e) override
    {
        if (e.button == Button1 && browser.release(e.x, e.y)) {
            update();
        }
    }

    void on_key(XKeyEvent& e) override
    {
        KeySym symbol = NoSymbol;
        const std::string typed = lookup(e, symbol);
        if ((e.state & ControlMask) != 0) {
            if (symbol == XK_q || symbol == XK_w) {
                quit();
            } else if (symbol == XK_v || symbol == XK_V) {
                paste(tiny::Clipboard::Which::Clipboard, e.time);
            }
            return;
        }
        if (symbol == XK_Insert && (e.state & ShiftMask) != 0) {
            paste(tiny::Clipboard::Which::Clipboard, e.time);
            return;
        }
        if (browser.view() == FontBrowser::View::Detail && symbol == XK_i &&
            (e.state & ControlMask) == 0) {
            if (browser.key(FontBrowser::Key::Info)) {
                update();
            }
            return;
        }
        static const std::pair<KeySym, FontBrowser::Key> keys[] = {
                {XK_Left, FontBrowser::Key::Left},
                {XK_Right, FontBrowser::Key::Right},
                {XK_Up, FontBrowser::Key::Up},
                {XK_Down, FontBrowser::Key::Down},
                {XK_Page_Up, FontBrowser::Key::PageUp},
                {XK_Page_Down, FontBrowser::Key::PageDown},
                {XK_Home, FontBrowser::Key::Home},
                {XK_End, FontBrowser::Key::End},
                {XK_Return, FontBrowser::Key::Return},
                {XK_KP_Enter, FontBrowser::Key::Return},
                {XK_BackSpace, FontBrowser::Key::Back},
                {XK_Escape, FontBrowser::Key::Escape}};
        for (const auto& [code, key] : keys) {
            if (symbol == code) {
                if (browser.key(key)) {
                    update();
                }
                return;
            }
        }
        if (browser.type(typed)) { // the search of the overview
            update();
        }
    }

    //! Type the text of a selection into the search
    void paste(tiny::Clipboard::Which which, Time time)
    {
        clipboard().request(
                which,
                [this](const std::string& text) {
                    if (browser.type(tiny::single_line(text))) {
                        update();
                    }
                },
                time);
    }

    void on_button_press(const XButtonEvent& e) override
    {
        if (e.button == Button1) {
            browser.press(e.x, e.y);
        } else if (e.button == Button2) {
            paste(tiny::Clipboard::Which::Primary, e.time);
        } else if ((e.button == Button4 || e.button == Button5) &&
                   browser.wheel(e.button == Button4 ? -1 : 1)) {
            update();
        }
    }

    //! `latin` when the font has the letters A and a, else its first
    //! characters from `skip` (a font of kanji or of icons has no letters)
    static std::string sample_of(const XftFont* f, size_t skip,
                                 const std::string& latin, size_t count)
    {
        if (FcCharSetHasChar(f->charset, 'A') &&
            FcCharSetHasChar(f->charset, 'a')) {
            return latin;
        }
        return tiny::xdg::font_sample(f->charset, skip, count);
    }

    //! Where the lines of the specimen are, and how high they are together
    void measure_detail()
    {
        measured = browser.family();
        baselines.clear();
        lines = FontBrowser::specimen(metrics);
        int y = metrics.margin;
        for (size_t i = 0; i < lines.size(); ++i) {
            FontBrowser::Line& line = lines[i];
            const XftFont* f = font(measured, line.size);
            if (f != nullptr) {
                // the letters, the capitals and the digits have the same
                // number of characters in a font without a Latin alphabet
                static const size_t skips[] = {0, 26, 52};
                static const size_t counts[] = {26, 26, 40};
                line.text = sample_of(f, i < 3 ? skips[i] : 0, line.text,
                                      i < 3 ? counts[i] : 24);
            }
            const int ascent = f != nullptr ? f->ascent : line.size;
            const int descent = f != nullptr ? f->descent : 0;
            baselines.push_back(y + ascent);
            y += ascent + descent + metrics.line_gap;
        }
        browser.set_detail_height(y - metrics.line_gap + metrics.margin);
    }

    void paint() override
    {
        if (browser.view() == FontBrowser::View::Overview) {
            measured.clear(); // the specimen is measured at every opening
            info_measured.clear();
        } else if (browser.view() == FontBrowser::View::Info) {
            if (info_measured != browser.family()) {
                measure_info();
            }
        } else {
            info_measured.clear(); // the info is measured at every opening
            if (measured.empty()) {
                measure_detail();
            }
        }
        // the fonts of the cells that show are never thrown away mid-paint
        const auto [first, end] = browser.visible_cells();
        if (cached_fonts() > font_cache_limit + 2 * (end - first)) {
            close_fonts();
        }
        fill(white, {0, 0, width, height});
        if (browser.view() == FontBrowser::View::Detail) {
            paint_detail();
        } else if (browser.view() == FontBrowser::View::Info) {
            paint_info();
        } else {
            paint_overview();
        }
    }

    //! The search field at the top: the text and a cursor, or a hint
    void paint_search()
    {
        const tiny::Rect r = browser.search_rect();
        fill(white, r);
        frame(rule, r, 1);
        if (ui_font == nullptr) {
            return;
        }
        const std::string& text = browser.search_field().text();
        const int padding = metrics.margin / 2;
        const int room = r.width - 2 * padding;
        if (text.empty()) {
            text_in(ui_font, dim, r, padding, "Search fonts");
        } else {
            // a long text shows its end
            const int shift = std::min(0, room - text_width(ui_font, text));
            text_in(ui_font, black, r, padding, text, shift);
        }
        const int shown = std::min(room, text_width(ui_font, text));
        fill(black, {r.x + padding + shown,
                     r.y + (r.height - ui_font->ascent - ui_font->descent) / 2,
                     1, ui_font->ascent + ui_font->descent});
    }

    void paint_overview()
    {
        if (browser.families().empty()) {
            paint_search();
            if (ui_font != nullptr) {
                const std::string text = browser.search_field().text().empty()
                                                 ? "No fonts found"
                                                 : "No font matches";
                text_at(ui_font, black, (width - text_width(ui_font, text)) / 2,
                        height / 2, text);
            }
            return;
        }
        const int inset = metrics.margin / 3;
        const auto [first, end] = browser.visible_cells();
        for (size_t i = first; i < end; ++i) {
            const tiny::Rect cell = browser.cell_rect(i);
            const std::string& family = browser.families()[i];
            if (i == browser.selected()) {
                fill(selected,
                     {cell.x + inset, cell.y + inset, cell.width - 2 * inset,
                      cell.height - 2 * inset});
            }
            clip({cell.x + inset, cell.y, cell.width - 2 * inset, cell.height});
            if (XftFont* f = font(family, metrics.preview)) {
                const std::string sample = sample_of(f, 0, "Aa", 2);
                const int x = std::max(
                        cell.x + inset,
                        cell.x + (cell.width - text_width(f, sample)) / 2);
                text_at(f, black, x, cell.y + metrics.preview_baseline, sample);
            }
            if (ui_font != nullptr) {
                const std::string name =
                        fit(ui_font, family, cell.width - 2 * inset);
                text_at(ui_font, black,
                        cell.x + (cell.width - text_width(ui_font, name)) / 2,
                        cell.y + metrics.name_baseline, name);
            }
            unclip();
        }
        // the cells scroll under the bar
        fill(white, {0, 0, width, browser.top()});
        paint_search();
    }

    //! The header of the detail and the info: the breadcrumbs at the left
    //! ("‹ Fonts", in the info also "‹ Family"), the title in the middle and
    //! the button Info at the right end (in the detail)
    void paint_header(const std::string& heading, bool in_info)
    {
        const int head = metrics.header;
        if (ui_font != nullptr) {
            const int baseline =
                    (head + ui_font->ascent - ui_font->descent) / 2;
            // "‹ " points back
            text_at(ui_font, black, metrics.margin, baseline,
                    "\xE2\x80\xB9 Fonts");
            int left = metrics.back_width;
            if (in_info) {
                const tiny::Rect crumb = browser.crumb_rect();
                const std::string name =
                        fit(ui_font, "\xE2\x80\xB9 " + browser.family(),
                            crumb.width - metrics.margin);
                text_at(ui_font, black, crumb.x + metrics.margin, baseline,
                        name);
                left = crumb.x + crumb.width;
            } else {
                const tiny::Rect info = browser.info_rect();
                const std::string label = "Info";
                text_at(ui_font, black,
                        info.x + (info.width - text_width(ui_font, label)) / 2,
                        baseline, label);
                frame(rule,
                      {info.x + metrics.margin / 2, head / 5,
                       info.width - metrics.margin, head * 3 / 5},
                      1);
            }
            const int room = width - 2 * left;
            if (room > 0) {
                const std::string name = fit(ui_font, heading, room);
                text_at(ui_font, black, (width - text_width(ui_font, name)) / 2,
                        baseline, name);
            }
        }
        fill(rule, {0, head - 1, width, 1});
    }

    //! The rows of the info as lines of text: a title of a group, or a label
    //! with the first line of the value, and the other lines of the value
    void measure_info()
    {
        info_measured = browser.family();
        info_lines.clear();
        const int label_room = std::max(1, width / 4);
        const int value_x = metrics.margin + label_room;
        const int value_room = std::max(1, width - value_x - metrics.margin);
        int y = metrics.margin;
        const int line_height =
                ui_font != nullptr ? ui_font->ascent + ui_font->descent : 1;
        const int ascent = ui_font != nullptr ? ui_font->ascent : 1;
        for (const tiny::xdg::InfoRow& row :
             tiny::xdg::font_info(browser.family())) {
            if (row.heading) {
                y += line_height / 2;
                info_lines.push_back(
                        {row.label, true, y + ascent, std::string()});
                y += line_height + metrics.line_gap;
                continue;
            }
            bool first = true;
            for (const std::string& part :
                 wrap(ui_font, row.value, value_room)) {
                info_lines.push_back({first ? row.label : std::string(), false,
                                      y + ascent, part});
                first = false;
                y += line_height;
            }
            y += metrics.line_gap;
        }
        if (info_lines.empty()) {
            info_lines.push_back(
                    {"No information", false, y + ascent, std::string()});
            y += line_height;
        }
        browser.set_detail_height(y + metrics.margin);
    }

    void paint_info()
    {
        const int head = metrics.header;
        paint_header("Info", true);
        if (ui_font == nullptr) {
            return;
        }
        const int value_x = metrics.margin + std::max(1, width / 4);
        clip({0, head, width, height - head});
        for (const InfoLine& line : info_lines) {
            const int baseline = head + line.baseline - browser.scroll();
            if (baseline < head - ui_font->ascent ||
                baseline > height + ui_font->descent) {
                continue;
            }
            if (line.heading) {
                text_at(ui_font, black, metrics.margin, baseline, line.label);
                continue;
            }
            if (!line.label.empty()) {
                text_at(ui_font, dim, metrics.margin, baseline, line.label);
            }
            text_at(ui_font, black, value_x, baseline, line.value);
        }
        unclip();
    }

    void paint_detail()
    {
        const int head = metrics.header;
        paint_header(browser.family(), false);

        clip({0, head, width, height - head});
        for (size_t i = 0; i < lines.size() && i < baselines.size(); ++i) {
            const int baseline = head + baselines[i] - browser.scroll();
            // skip a line that is out of the window (a big one is tall)
            if (baseline < head - 2 * lines[i].size ||
                baseline > height + 2 * lines[i].size) {
                continue;
            }
            if (XftFont* f = font(measured, lines[i].size)) {
                text_at(f, black, metrics.margin, baseline, lines[i].text);
            }
        }
        unclip();
    }

    FontBrowser::Metrics metrics;
    FontBrowser browser;
    XftFont* ui_font = nullptr;
    XftColor& black;
    XftColor& white;
    XftColor& selected;
    XftColor& rule;
    XftColor& dim;
    //! A line of the info: the label (only in the first line of a row), if it
    //! is the title of a group, the baseline in the content and the text of the
    //! value
    struct InfoLine {
        std::string label;
        bool heading = false;
        int baseline = 0;
        std::string value;
    };
    std::string info_measured; //!< the family that `info_lines` belong to
    std::vector<InfoLine> info_lines;
    std::string measured; //!< the family that `baselines` belong to
    std::vector<FontBrowser::Line> lines; //!< the specimen of `measured`
    std::vector<int> baselines;           //!< of the lines of the specimen
};

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1) {
        const bool help = std::strcmp(argv[1], "--help") == 0;
        fprintf(help ? stdout : stderr,
                "usage: %s\nShows the fonts of the system. Click a font to "
                "see its specimen.\n",
                argv[0]);
        return help ? EXIT_SUCCESS : 2;
    }
    setlocale(LC_CTYPE, "");

    // the scale of the shell, so that the window fits a high-DPI screen
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
        FontsApp app(tiny::get_display(), tiny::xdg::font_families(),
                     config.scale);
        app.run();
    } catch (const std::exception& e) {
        fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
