// gtest must be included before Xlib: X.h defines the None macro
#include <gtest/gtest.h>

#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "display.h"
#include "x_util.h"

static XEvent make_event(int type)
{
    XEvent e = {};
    e.type = type;
    return e;
}

TEST(EventToString, names_of_known_events)
{
    const struct {
        int type;
        const char* name;
    } cases[] = {
            {KeyPress, "KeyPress"},
            {KeyRelease, "KeyRelease"},
            {ButtonPress, "ButtonPress"},
            {ButtonRelease, "ButtonRelease"},
            {MotionNotify, "MotionNotify"},
            {EnterNotify, "EnterNotify"},
            {LeaveNotify, "LeaveNotify"},
            {FocusIn, "FocusIn"},
            {FocusOut, "FocusOut"},
            {KeymapNotify, "KeymapNotify"},
            {Expose, "Expose"},
            {GraphicsExpose, "GraphicsExpose"},
            {NoExpose, "NoExpose"},
            {VisibilityNotify, "VisibilityNotify"},
            {CreateNotify, "CreateNotify"},
            {DestroyNotify, "DestroyNotify"},
            {UnmapNotify, "UnmapNotify"},
            {MapNotify, "MapNotify"},
            {MapRequest, "MapRequest"},
            {ReparentNotify, "ReparentNotify"},
            {ConfigureNotify, "ConfigureNotify"},
            {ConfigureRequest, "ConfigureRequest"},
            {GravityNotify, "GravityNotify"},
            {ResizeRequest, "ResizeRequest"},
            {CirculateNotify, "CirculateNotify"},
            {CirculateRequest, "CirculateRequest"},
            {PropertyNotify, "PropertyNotify"},
            {SelectionClear, "SelectionClear"},
            {SelectionRequest, "SelectionRequest"},
            {SelectionNotify, "SelectionNotify"},
            {ColormapNotify, "ColormapNotify"},
            {ClientMessage, "ClientMessage"},
            {MappingNotify, "MappingNotify"},
            {GenericEvent, "GenericEvent"},
            {LASTEvent, "LASTEvent"},
    };

    for (auto c : cases) {
        EXPECT_STREQ(event_to_string(make_event(c.type)), c.name);
    }
}

TEST(EventToString, unknown_type)
{
    EXPECT_STREQ(event_to_string(make_event(0)), "Unknown type");
    EXPECT_STREQ(event_to_string(make_event(LASTEvent + 10)), "Unknown type");
}

TEST(WindowOfEvent, events_with_window_field)
{
    XEvent e = make_event(KeyPress);
    e.xkey.window = 11;
    EXPECT_EQ(tiny::window_of_event(e), 11u);

    e = make_event(KeyRelease);
    e.xkey.window = 12;
    EXPECT_EQ(tiny::window_of_event(e), 12u);

    e = make_event(ButtonPress);
    e.xbutton.window = 13;
    EXPECT_EQ(tiny::window_of_event(e), 13u);

    e = make_event(ButtonRelease);
    e.xbutton.window = 14;
    EXPECT_EQ(tiny::window_of_event(e), 14u);

    e = make_event(MotionNotify);
    e.xmotion.window = 15;
    EXPECT_EQ(tiny::window_of_event(e), 15u);

    e = make_event(EnterNotify);
    e.xcrossing.window = 16;
    EXPECT_EQ(tiny::window_of_event(e), 16u);

    e = make_event(LeaveNotify);
    e.xcrossing.window = 17;
    EXPECT_EQ(tiny::window_of_event(e), 17u);

    e = make_event(FocusIn);
    e.xfocus.window = 18;
    EXPECT_EQ(tiny::window_of_event(e), 18u);

    e = make_event(FocusOut);
    e.xfocus.window = 19;
    EXPECT_EQ(tiny::window_of_event(e), 19u);

    e = make_event(KeymapNotify);
    e.xkeymap.window = 20;
    EXPECT_EQ(tiny::window_of_event(e), 20u);

    e = make_event(Expose);
    e.xexpose.window = 21;
    EXPECT_EQ(tiny::window_of_event(e), 21u);

    e = make_event(VisibilityNotify);
    e.xvisibility.window = 22;
    EXPECT_EQ(tiny::window_of_event(e), 22u);

    e = make_event(ResizeRequest);
    e.xresizerequest.window = 23;
    EXPECT_EQ(tiny::window_of_event(e), 23u);

    e = make_event(PropertyNotify);
    e.xproperty.window = 24;
    EXPECT_EQ(tiny::window_of_event(e), 24u);

    e = make_event(SelectionClear);
    e.xselectionclear.window = 25;
    EXPECT_EQ(tiny::window_of_event(e), 25u);

    e = make_event(ColormapNotify);
    e.xcolormap.window = 26;
    EXPECT_EQ(tiny::window_of_event(e), 26u);

    e = make_event(ClientMessage);
    e.xclient.window = 27;
    EXPECT_EQ(tiny::window_of_event(e), 27u);
}

TEST(WindowOfEvent, drawable_and_requestor_events)
{
    XEvent e = make_event(GraphicsExpose);
    e.xgraphicsexpose.drawable = 31;
    EXPECT_EQ(tiny::window_of_event(e), 31u);

    e = make_event(NoExpose);
    e.xnoexpose.drawable = 32;
    EXPECT_EQ(tiny::window_of_event(e), 32u);

    e = make_event(SelectionRequest);
    e.xselectionrequest.requestor = 33;
    EXPECT_EQ(tiny::window_of_event(e), 33u);

    e = make_event(SelectionNotify);
    e.xselection.requestor = 34;
    EXPECT_EQ(tiny::window_of_event(e), 34u);
}

// These events carry the receiving window in `event`/`parent` and the
// affected window in `window`; window_of_event must return the latter.
TEST(WindowOfEvent, structure_events_return_the_affected_window)
{
    XEvent e = make_event(CreateNotify);
    e.xcreatewindow.parent = 1;
    e.xcreatewindow.window = 41;
    EXPECT_EQ(tiny::window_of_event(e), 41u);

    e = make_event(DestroyNotify);
    e.xdestroywindow.event = 1;
    e.xdestroywindow.window = 42;
    EXPECT_EQ(tiny::window_of_event(e), 42u);

    e = make_event(UnmapNotify);
    e.xunmap.event = 1;
    e.xunmap.window = 43;
    EXPECT_EQ(tiny::window_of_event(e), 43u);

    e = make_event(MapNotify);
    e.xmap.event = 1;
    e.xmap.window = 44;
    EXPECT_EQ(tiny::window_of_event(e), 44u);

    e = make_event(MapRequest);
    e.xmaprequest.parent = 1;
    e.xmaprequest.window = 45;
    EXPECT_EQ(tiny::window_of_event(e), 45u);

    e = make_event(ReparentNotify);
    e.xreparent.event = 1;
    e.xreparent.window = 46;
    EXPECT_EQ(tiny::window_of_event(e), 46u);

    e = make_event(ConfigureNotify);
    e.xconfigure.event = 1;
    e.xconfigure.window = 47;
    EXPECT_EQ(tiny::window_of_event(e), 47u);

    e = make_event(ConfigureRequest);
    e.xconfigurerequest.parent = 1;
    e.xconfigurerequest.window = 48;
    EXPECT_EQ(tiny::window_of_event(e), 48u);

    e = make_event(GravityNotify);
    e.xgravity.event = 1;
    e.xgravity.window = 49;
    EXPECT_EQ(tiny::window_of_event(e), 49u);

    e = make_event(CirculateNotify);
    e.xcirculate.event = 1;
    e.xcirculate.window = 50;
    EXPECT_EQ(tiny::window_of_event(e), 50u);

    e = make_event(CirculateRequest);
    e.xcirculaterequest.parent = 1;
    e.xcirculaterequest.window = 51;
    EXPECT_EQ(tiny::window_of_event(e), 51u);
}

TEST(WindowOfEvent, events_without_window)
{
    EXPECT_EQ(tiny::window_of_event(make_event(MappingNotify)),
              static_cast<::Window>(None));
    EXPECT_EQ(tiny::window_of_event(make_event(GenericEvent)),
              static_cast<::Window>(None));
    EXPECT_EQ(tiny::window_of_event(make_event(0)),
              static_cast<::Window>(None));
}

TEST(XPoint, x_point_sets_coordinates)
{
    const XPoint p = tiny::x_point(-3, 7);

    EXPECT_EQ(p.x, -3);
    EXPECT_EQ(p.y, 7);
}

// ---- key grabs ----

static int grab_errors = 0;
static unsigned char last_error_code = 0;

static int count_errors(::Display*, XErrorEvent* e)
{
    ++grab_errors;
    last_error_code = e->error_code;
    return 0;
}

class GrabTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        display = tiny::get_display();
        other = XOpenDisplay(XDisplayString(display));
        ASSERT_NE(other, nullptr);
        root = XDefaultRootWindow(display);
        keycode = XKeysymToKeycode(display, XK_F2);
        old_handler = XSetErrorHandler(count_errors);
        grab_errors = 0;
    }

    void TearDown() override
    {
        XSync(display, False);
        XSetErrorHandler(old_handler);
        XCloseDisplay(other);
    }

    // The other client tries to grab every Num Lock / Caps Lock variant.
    int grab_from_other_client()
    {
        const unsigned int mods[] = {Mod1Mask, Mod1Mask | Mod2Mask,
                                     Mod1Mask | LockMask,
                                     Mod1Mask | Mod2Mask | LockMask};
        grab_errors = 0;
        for (const unsigned int m : mods) {
            XGrabKey(other, keycode, m, root, True, GrabModeAsync,
                     GrabModeAsync);
        }
        XSync(other, False);
        return grab_errors;
    }

    void release_from_other_client()
    {
        XUngrabKey(other, keycode, AnyModifier, root);
        XSync(other, False);
    }

    ::Display* display;
    ::Display* other;
    ::Window root;
    int keycode;
    XErrorHandler old_handler;
};

TEST_F(GrabTest, grab_blocks_other_clients_for_all_lock_modifiers)
{
    tiny::x_grab_key(display, keycode, Mod1Mask, root);
    XSync(display, False);

    EXPECT_EQ(grab_from_other_client(), 4);
    EXPECT_EQ(last_error_code, BadAccess);

    tiny::x_ungrab_key(display, keycode, Mod1Mask, root);
    XSync(display, False);
}

TEST_F(GrabTest, ungrab_releases_all_lock_modifiers)
{
    tiny::x_grab_key(display, keycode, Mod1Mask, root);
    tiny::x_ungrab_key(display, keycode, Mod1Mask, root);
    XSync(display, False);

    EXPECT_EQ(grab_from_other_client(), 0);

    release_from_other_client();
}

// ---- resize_pixmap ----

class PixmapTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        display = tiny::get_display();
        root = XDefaultRootWindow(display);
    }

    Pixmap filled(unsigned int w, unsigned int h, unsigned int depth,
                  unsigned long color)
    {
        const Pixmap pm = XCreatePixmap(display, root, w, h, depth);
        GC gc = XCreateGC(display, pm, 0, nullptr);
        XSetForeground(display, gc, color);
        XFillRectangle(display, pm, gc, 0, 0, w, h);
        XFreeGC(display, gc);
        return pm;
    }

    void geometry(Pixmap pm, unsigned int& w, unsigned int& h,
                  unsigned int& depth)
    {
        ::Window r;
        int x;
        int y;
        unsigned int border;
        XSync(display, False);
        XGetGeometry(display, pm, &r, &x, &y, &w, &h, &border, &depth);
    }

    ::Display* display;
    ::Window root;
};

TEST_F(PixmapTest, resize_creates_pixmap_of_target_size)
{
    const int depth = DefaultDepth(display, 0);
    const Pixmap origin = filled(100, 100, tiny::x_uint(depth), 0xFF0000);

    const Pixmap target = tiny::resize_pixmap(display, root, origin, 100, 100,
                                              50, 50, tiny::x_uint(depth));

    unsigned int w;
    unsigned int h;
    unsigned int d;
    geometry(target, w, h, d);
    EXPECT_EQ(w, 50u);
    EXPECT_EQ(h, 50u);
    EXPECT_EQ(d, static_cast<unsigned int>(depth));
    XFreePixmap(display, origin);
    XFreePixmap(display, target);
}

TEST_F(PixmapTest, resize_keeps_color)
{
    const int depth = DefaultDepth(display, 0);
    const Pixmap origin = filled(100, 100, tiny::x_uint(depth), 0xFF0000);

    const Pixmap target = tiny::resize_pixmap(display, root, origin, 100, 100,
                                              50, 50, tiny::x_uint(depth));

    XSync(display, False);
    XImage* img = XGetImage(display, target, 10, 10, 1, 1, AllPlanes, ZPixmap);
    EXPECT_EQ(XGetPixel(img, 0, 0) & 0xFFFFFF, 0xFF0000u);
    XDestroyImage(img);
    XFreePixmap(display, origin);
    XFreePixmap(display, target);
}

TEST_F(PixmapTest, resize_monochrome_pixmap)
{
    const Pixmap origin = filled(64, 32, 1, 1);

    const Pixmap target =
            tiny::resize_pixmap(display, root, origin, 64, 32, 16, 8, 1);

    unsigned int w;
    unsigned int h;
    unsigned int d;
    geometry(target, w, h, d);
    EXPECT_EQ(w, 16u);
    EXPECT_EQ(h, 8u);
    EXPECT_EQ(d, 1u);
    XFreePixmap(display, origin);
    XFreePixmap(display, target);
}
