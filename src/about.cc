// tiny-about: the window that says what Tiny Shell is.
//
// The logo, the name, the version, the system, the license and the address of
// the project. It is a window of its own (not a part of the shell), so the
// dock shows the logo for it like for any application. Escape, Return,
// Ctrl+Q, Ctrl+W and the button Close end it.

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cmath>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "app/about.h"
#include "display.h"
#include "image/image.h"
#include "image/pixmap.h"
#include "wm/config.h"
#include "wm/options.h"
#include "wm/system_info.h"
#include "x_util.h"
#include "xdg/application_index.h"
#include "xdg/dirs.h"
#include "xdg/icon_theme.h"
#include "xft_window.h"

namespace {

constexpr const char* program = "tiny-about";

//! The sizes at the scale 1
constexpr int margin = 24;
constexpr int logo_size = 96;
constexpr int title_size = 26;
constexpr int line_size = 14;
constexpr int line_gap = 6;
constexpr int button_width = 120;
constexpr int button_height = 32;
constexpr int window_width = 380;

class AboutApp : public tiny::XftWindow {
  public:
    AboutApp(tiny::Display& display_arg, double scale):
        XftWindow(display_arg),
        factor(scale),
        lines(tiny::app::about_lines(tiny::wm::version_number(),
                                     tiny::wm::system_info())),
        black(alloc_gray(0)),
        white(alloc_gray(255)),
        rule(alloc_gray(0xc8)),
        selected(alloc_gray(0xe8)),
        dim(alloc_gray(0x6e))
    {
        const int lines_height =
                static_cast<int>(lines.size()) * (px(line_size) + px(line_gap));
        const int w = px(window_width);
        const int h = px(margin) + px(logo_size) + px(margin) + px(title_size) +
                      px(line_gap) + lines_height + px(margin) +
                      px(button_height) + px(margin);
        create({tiny::app::about_title, program, "TinyAbout", w, h, w, h, 0,
                false});
        load_logo();
    }

    ~AboutApp() override
    {
        if (logo != None) {
            XRenderFreePicture(display, logo);
        }
        if (logo_pixmap != None) {
            XFreePixmap(display, logo_pixmap);
        }
    }

  protected:
    void on_resize(int, int) override {}

    void on_key(XKeyEvent& e) override
    {
        KeySym symbol = NoSymbol;
        lookup(e, symbol);
        const bool control = (e.state & ControlMask) != 0;
        if (symbol == XK_Escape || symbol == XK_Return ||
            symbol == XK_KP_Enter ||
            (control && (symbol == XK_q || symbol == XK_w))) {
            quit();
        }
    }

    void on_button_release(const XButtonEvent& e) override
    {
        const tiny::Rect r = button_rect();
        if (e.button == Button1 && e.x >= r.x && e.x < r.x + r.width &&
            e.y >= r.y && e.y < r.y + r.height) {
            quit();
        }
    }

    void paint() override
    {
        fill(white, {0, 0, width, height});
        int y = px(margin);
        if (logo != None) {
            draw_logo(y);
        }
        y += px(logo_size) + px(margin);

        XftFont* title_font = font_by_name("sans:bold:pixelsize=" +
                                           std::to_string(px(title_size)));
        XftFont* line_font = font("sans", px(line_size));
        if (title_font != nullptr) {
            text_at(title_font, black,
                    (width - text_width(title_font, tiny::app::about_title)) /
                            2,
                    y + title_font->ascent, tiny::app::about_title);
        }
        y += px(title_size) + 2 * px(line_gap);
        if (line_font != nullptr) {
            for (const std::string& line : lines) {
                const std::string shown =
                        fit(line_font, line, width - 2 * px(margin));
                text_at(line_font, dim,
                        (width - text_width(line_font, shown)) / 2,
                        y + line_font->ascent, shown);
                y += px(line_size) + px(line_gap);
            }
        }
        const tiny::Rect r = button_rect();
        fill(selected, r);
        frame(rule, r, 1);
        if (line_font != nullptr) {
            text_at(line_font, black,
                    r.x + (r.width - text_width(line_font, "Close")) / 2,
                    r.y + (r.height + line_font->ascent - line_font->descent) /
                                    2,
                    "Close");
        }
    }

  private:
    int px(int value) const
    {
        return std::max(1, static_cast<int>(std::lround(value * factor)));
    }

    tiny::Rect button_rect() const
    {
        return {(width - px(button_width)) / 2,
                height - px(margin) - px(button_height), px(button_width),
                px(button_height)};
    }

    //! The icon `tiny-shell` of the hicolor theme, if it is installed
    void load_logo()
    {
        const auto dirs = tiny::xdg::default_data_dirs();
        std::vector<std::string> icon_dirs;
        std::vector<std::string> pixmaps;
        for (const std::string& dir : dirs) {
            icon_dirs.push_back(dir + "/icons");
            pixmaps.push_back(dir + "/pixmaps");
        }
        const tiny::xdg::IconTheme theme(icon_dirs, pixmaps);
        const int size = px(logo_size);
        const std::string file = theme.find("tiny-shell", size);
        const auto loaded = file.empty() ? std::nullopt
                                         : tiny::image::load_image(file, size);
        if (!loaded) {
            return;
        }
        // the icon of the window: the shell may not find the desktop file
        std::vector<tiny::image::Image> icons = {*loaded};
        if (const auto small = tiny::image::load_image(file, 48)) {
            icons.push_back(*small);
        }
        set_icon(icons);
        logo_pixmap = tiny::image::create_pixmap(
                display, XDefaultRootWindow(display), *loaded);
        if (logo_pixmap != None) {
            logo = tiny::image::create_argb_picture(display, logo_pixmap);
            logo_width = loaded->width;
            logo_height = loaded->height;
        }
    }

    void draw_logo(int y)
    {
        const XRenderPictFormat* format = XRenderFindVisualFormat(
                display, XDefaultVisual(display, screen));
        if (format == nullptr) {
            return;
        }
        const Picture target =
                XRenderCreatePicture(display, buffer, format, 0, nullptr);
        XRenderComposite(display, PictOpOver, logo, None, target, 0, 0, 0, 0,
                         (width - logo_width) / 2, y, tiny::x_uint(logo_width),
                         tiny::x_uint(logo_height));
        XRenderFreePicture(display, target);
    }

    double factor;
    std::vector<std::string> lines;
    Pixmap logo_pixmap = None;
    Picture logo = None;
    int logo_width = 0;
    int logo_height = 0;
    XftColor& black;
    XftColor& white;
    XftColor& rule;
    XftColor& selected;
    XftColor& dim;
};

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1) {
        const bool help = std::strcmp(argv[1], "--help") == 0;
        fprintf(help ? stdout : stderr,
                "usage: %s\nShows the version of Tiny Shell and of the "
                "system.\n",
                argv[0]);
        return help ? EXIT_SUCCESS : 2;
    }
    setlocale(LC_CTYPE, "");

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
        AboutApp app(tiny::get_display(), config.scale);
        app.run();
    } catch (const std::exception& e) {
        fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
