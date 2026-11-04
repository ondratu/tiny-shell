// tiny-fonts: lists the fonts of the system and shows a specimen of one.
//
// The overview is a grid with "Aa" in every font family and its name under it.
// A click (or Return) on a family opens its specimen: the letters, the digits
// and the signs, then a sentence in growing sizes. Escape, backspace or the
// button at the top return to the overview. Ctrl+Q and Ctrl+W quit.
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

#include "display.h"
#include "font_browser.h"
#include "style.h"
#include "wm/config.h"
#include "x_util.h"
#include "xdg/font_list.h"
#include "xft_window.h"

namespace {

using tiny::FontBrowser;

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
        rule(alloc_gray(0xc8))
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
                metrics.cell_width + 2 * margin, metrics.cell_height});
    }

    ~FontsApp() override
    {
        if (ui_font != nullptr) {
            XftFontClose(display, ui_font);
        }
    }

  private:
    void on_resize(int w, int h) override { browser.resize(w, h); }

    void on_button_release(const XButtonEvent& e) override
    {
        if (e.button == Button1 && browser.release(e.x, e.y)) {
            update();
        }
    }

    void on_key(XKeyEvent& e) override
    {
        KeySym symbol = NoSymbol;
        lookup(e, symbol);
        if ((e.state & ControlMask) != 0 &&
            (symbol == XK_q || symbol == XK_w)) {
            quit();
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
    }

    void on_button_press(const XButtonEvent& e) override
    {
        if (e.button == Button1) {
            browser.press(e.x, e.y);
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
        } else if (measured.empty()) {
            measure_detail();
        }
        // the fonts of the cells that show are never thrown away mid-paint
        const auto [first, end] = browser.visible_cells();
        if (cached_fonts() > font_cache_limit + 2 * (end - first)) {
            close_fonts();
        }
        fill(white, {0, 0, width, height});
        if (browser.view() == FontBrowser::View::Detail) {
            paint_detail();
        } else {
            paint_overview();
        }
    }

    void paint_overview()
    {
        if (browser.families().empty()) {
            if (ui_font != nullptr) {
                const std::string text = "No fonts found";
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
    }

    void paint_detail()
    {
        const int head = metrics.header;
        if (ui_font != nullptr) {
            const int baseline =
                    (head + ui_font->ascent - ui_font->descent) / 2;
            // "‹ " points back; the family is centered in the header
            text_at(ui_font, black, metrics.margin, baseline,
                    "\xE2\x80\xB9 Fonts");
            const int room = width - 2 * metrics.back_width;
            const std::string name = fit(ui_font, browser.family(), room);
            text_at(ui_font, black, (width - text_width(ui_font, name)) / 2,
                    baseline, name);
        }
        fill(rule, {0, head - 1, width, 1});

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
