#include <X11/cursorfont.h>

#include "wm_title.h"
#include "wm_theme.h"

#define SCREEN 0
#define NO_TITLE "Unknown Application"

const char * XFT_FONT = "Cantarell-15:bold";
// const char * BASE_FONT = "-*-lucidatypewriter-bold-*-*-*-*-*-*-*-*-90-*-*";
// const char * FIXED_FONT = "-*-fixed-bold-*-*-*-*-*-*-*-*-90-*-*";

WMTitle::WMTitle(Display * display, Window parent,
            int x, int y, int width, int height):
        WMWidget(display, parent, x, y, width, height,
                 0, 0x0, WM_WIN_BACKGROUND)
{
    XSetWindowAttributes attrs;
    attrs.win_gravity = NorthWestGravity;
    XChangeWindowAttributes(display, window, CWWinGravity , &attrs);

    text = NO_TITLE;
    // gc = XCreateGC(display, window, 0, nullptr);
    // XSetForeground(display, gc, WM_TITLE_FG_ENE);

    /*
    font = XLoadQueryFont(display, BASE_FONT);
    if (!font) {
        fprintf(stderr, "unable to load font %s: using fixed\n", BASE_FONT);
        font = XLoadQueryFont(display, FIXED_FONT);
    }
    XSetFont(display, gc, font->fid);
    */

    xft_font = XftFontOpenName(display, SCREEN, XFT_FONT);
}

WMTitle::~WMTitle()
{
    // XFreeGC(display, gc);
    // XFreeFont(display, font);

    XUngrabButton(display, Button1, AnyModifier, window);
    disconnect(ButtonPress);
    disconnect(ButtonRelease);
    disconnect(MotionNotify);
    disconnect(Expose);
}

void WMTitle::set_events()
{
    XSelectInput(display, window, ExposureMask);

    XGrabButton(            // Click to WM_header
        display, Button1, AnyModifier, window, true,
        ButtonPressMask | ButtonReleaseMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1));

    connect(ButtonPress,
            static_cast<event_signal_t>(&WMTitle::on_button_press));
    connect(ButtonRelease,
            static_cast<event_signal_t>(&WMTitle::on_button_release));
    connect(MotionNotify,
            static_cast<event_signal_t>(&WMTitle::on_motion_notify));
    connect(Expose,
            static_cast<event_signal_t>(&WMTitle::on_expose));
}

void WMTitle::set_text(const std::string &text)
{
    printf("Text set as: %s (Obrázky)\n", text.c_str());
    this->text = text;
    if (is_maped){
        on_expose(XEvent(), nullptr);
    }
    // TODO: vydrazdit expose .. nevim jak na to, ale bude to treba :-)
}

void WMTitle::on_button_press(const XEvent &e, void * data)
{
    // XXX: mozna by stalo za to nastavit drag_n_drop a testovat ho
    // v destructoru...
    XGrabPointer(
        display, window, false,
        ButtonPressMask|ButtonReleaseMask|Button1MotionMask,
        GrabModeAsync, GrabModeAsync,
        None, XCreateFontCursor(display, XC_hand1), CurrentTime);

    on_drag_begin(this, e);
}

void WMTitle::on_button_release(const XEvent &e, void * data)
{
    XUngrabPointer(display, CurrentTime);

    on_drag_end(this, e);
}

void WMTitle::on_motion_notify(const XEvent &e, void * data)
{
    XEvent event = e;
    while (XCheckTypedWindowEvent(  // skip penging motion events
                display, event.xmotion.window, MotionNotify, &event))
    {}

    on_drag_motion(this, event);
}

void WMTitle::on_expose(const XEvent &e, void * data)
{
    int x,y;
    XWindowAttributes attrs;

    int direction;
    int ascent;
    int descent;
    XCharStruct overall;

    XftColor color;
    XftDraw *draw;
    XRenderColor kindofblue = { 0x4c00, 0x7800, 0x9900, 0xff00};
    XGlyphInfo extents;
    Visual *visual = DefaultVisual(display, SCREEN);
    Colormap colormap = DefaultColormap(display, SCREEN);

    //XftColorAllocValue(display, visual, colormap, &kindofblue, &color);
    XftColorAllocName(display, visual, colormap, WM_TITLE_FG_ENE, &color);
    draw = XftDrawCreate(display, window, visual, colormap);

    XGetWindowAttributes(display, window, &attrs);

    //XTextExtents(font, text.c_str(), text.size(),
    //             &direction, &ascent, &descent, &overall);

    XftTextExtentsUtf8(display, xft_font, (FcChar8*)text.c_str(), text.size(),
                       &extents);

    x = (attrs.width-extents.width)/2;
    if ((x + extents.width) > (attrs.width - 3*WM_WIN_HEADER)){
        x = attrs.width - 3*WM_WIN_HEADER - extents.width;
    }
    if (x < 0) {
        x = 0;
    }

    y = (attrs.height+extents.height)/2;
    // Real text height is not so hieght then extend computes
    //y = extents.height;

    XClearWindow(display, window);
    //XDrawString(display, window, gc, x, y, text.c_str(), text.size());
    XftDrawStringUtf8(draw, &color, xft_font, x, y,
                      (FcChar8*)text.c_str(), text.size());

    XftDrawDestroy(draw);
    XftColorFree(display, visual, colormap, &color);
}
