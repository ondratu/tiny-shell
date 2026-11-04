#pragma once

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <utility>

#include "display.h"
#include "input_method.h"
#include "widget.h"

namespace tiny {

/**
 * @short A top-level window that draws its text with Xft into a pixmap.
 *
 * The base of the programs that draw their own content (`tiny-fonts`,
 * `tiny-settings`): the window with its hints, a pixmap that is copied to the
 * window (so it does not flicker), the colors, a cache of fonts, the drawing
 * helpers and the event loop. What a program shows and does is in `paint()`
 * and the `on_*` functions; the logic that needs no X belongs to a plain class
 * of the program (`FontBrowser`, `SettingsForm`).
 */
class XftWindow {
  public:
    struct Spec {
        const char* title = "";
        const char* res_name = "";  //!< `WM_CLASS` instance
        const char* res_class = ""; //!< `WM_CLASS` class
        int width = 0;
        int height = 0;
        int min_width = 0;
        int min_height = 0;
        long events = 0;           //!< the events on top of the common ones
        bool input_method = false; //!< ask an input method for the typing
    };

    XftWindow(const XftWindow&) = delete;
    XftWindow& operator=(const XftWindow&) = delete;
    virtual ~XftWindow();

    //! Paint, show the window and handle the events until `quit()`
    void run();

    //! Handle one event; what `run()` does for each
    void dispatch(XEvent& e);

    void quit() { running = false; }

    int get_width() const { return width; }
    int get_height() const { return height; }
    ::Window get_window() const { return window; }

  protected:
    explicit XftWindow(Display& display);

    //! Make the window; call it when the program knows its size
    void create(const Spec& spec);

    //! Draw the whole content into the pixmap (`draw`)
    virtual void paint() = 0;
    //! The window got another size (the pixmap is made again after it)
    virtual void on_resize(int width, int height) = 0;
    virtual void on_key(XKeyEvent&) {}
    virtual void on_button_press(const XButtonEvent&) {}
    virtual void on_button_release(const XButtonEvent&) {}

    //! Paint and show
    void update();
    //! Copy the pixmap to the window
    void present();

    //! A color for `XftDraw`, freed with the window
    XftColor& alloc_color(unsigned red, unsigned green, unsigned blue);
    XftColor& alloc_gray(unsigned gray)
    {
        return alloc_color(gray, gray, gray);
    }

    //! A font of a family in pixels, or from an Xft name; both are cached and
    //! null when the font cannot be opened. Closing them (`close_fonts()`)
    //! invalidates all the pointers.
    XftFont* font(const std::string& family, int pixel_size);
    XftFont* font_by_name(const std::string& xft_name);
    size_t cached_fonts() const { return fonts.size(); }
    void close_fonts();

    int text_width(XftFont* f, const std::string& text) const;
    //! The text, cut with "…" when it is wider than `max_width`
    std::string fit(XftFont* f, const std::string& text, int max_width) const;

    void fill(const XftColor& color, const Rect& r);
    //! A frame of `line` pixels inside of the rectangle
    void frame(const XftColor& color, const Rect& r, int line);
    void clip(const Rect& r);
    void unclip();
    void text_at(XftFont* f, const XftColor& color, int x, int baseline,
                 const std::string& text);
    //! The text with its baseline in the middle of the rectangle, `left` from
    //! its start and moved by `shift` (to the left, to show the cursor)
    void text_in(XftFont* f, const XftColor& color, const Rect& r, int left,
                 const std::string& text, int shift = 0);

    //! The characters of a key in UTF-8, and its symbol
    std::string lookup(XKeyEvent& e, KeySym& symbol) const;

    Display& atoms; //!< the atoms of the standards
    ::Display* display;
    int screen;
    ::Window window = None;
    Pixmap buffer = None;
    XftDraw* draw = nullptr;
    int width = 0;
    int height = 0;

  private:
    void new_buffer();
    void on_configure(const XConfigureEvent& e);

    Atom delete_window = None;
    std::unique_ptr<InputMethod> input;
    std::vector<std::unique_ptr<XftColor>> colors; //!< the references stay
    //! (family, pixel size), or (Xft name, 0): a pixel size is never 0
    std::map<std::pair<std::string, int>, XftFont*> fonts;
    bool running = true;
};

} // namespace tiny
