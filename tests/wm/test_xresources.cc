#include <gtest/gtest.h>

#include <X11/Xatom.h>

#include <string>
#include <vector>

#include "display.h"
#include "wm/xresources.h"

using tiny::wm::merge_resources;
using tiny::wm::ResourceChange;
using tiny::wm::xft_resources;
using tiny::wm::XftOption;
using Changes = std::vector<ResourceChange>;

TEST(XResourcesTest, a_resource_that_is_there_is_replaced_and_the_others_stay)
{
    const std::string text = merge_resources(
            "*background: white\nXft.antialias: 1\nXft.dpi: 96\n",
            {{"Xft.antialias", "0"}});

    EXPECT_EQ(text, "*background: white\nXft.dpi: 96\nXft.antialias:\t0\n");
}

TEST(XResourcesTest, a_new_resource_comes_at_the_end_of_text_without_a_newline)
{
    EXPECT_EQ(merge_resources("a: b", {{"Xft.rgba", "rgb"}}),
              "a: b\nXft.rgba:\trgb\n");
    EXPECT_EQ(merge_resources("", {{"Xft.rgba", "rgb"}}), "Xft.rgba:\trgb\n");
}

TEST(XResourcesTest, an_empty_value_removes_the_resource)
{
    EXPECT_EQ(merge_resources("Xft.rgba:\tbgr\nXft.dpi: 96\n",
                              {{"Xft.rgba", ""}}),
              "Xft.dpi: 96\n");
}

TEST(XResourcesTest, the_name_is_the_whole_text_before_the_colon)
{
    // another name that starts the same, a comment and a line without a colon
    const std::string kept =
            "Xft.antialiasing: x\n! Xft.antialias: 1\nnonsense\n";

    EXPECT_EQ(merge_resources(kept, {{"Xft.antialias", "1"}}),
              kept + "Xft.antialias:\t1\n");
}

TEST(XResourcesTest, antialiasing_is_a_number)
{
    EXPECT_EQ(xft_resources(XftOption::Antialias, "yes"),
              (Changes{{"Xft.antialias", "1"}}));
    EXPECT_EQ(xft_resources(XftOption::Antialias, "no"),
              (Changes{{"Xft.antialias", "0"}}));
    EXPECT_EQ(xft_resources(XftOption::Antialias, ""),
              (Changes{{"Xft.antialias", ""}}));
}

TEST(XResourcesTest, hinting_sets_the_switch_and_the_style)
{
    EXPECT_EQ(xft_resources(XftOption::Hinting, "none"),
              (Changes{{"Xft.hinting", "0"}, {"Xft.hintstyle", "hintnone"}}));
    EXPECT_EQ(xft_resources(XftOption::Hinting, "slight"),
              (Changes{{"Xft.hinting", "1"}, {"Xft.hintstyle", "hintslight"}}));
    EXPECT_EQ(xft_resources(XftOption::Hinting, ""),
              (Changes{{"Xft.hinting", ""}, {"Xft.hintstyle", ""}}));
}

TEST(XResourcesTest, the_subpixel_order_is_kept_as_it_is)
{
    EXPECT_EQ(xft_resources(XftOption::Rgba, "vbgr"),
              (Changes{{"Xft.rgba", "vbgr"}}));
}

namespace {

std::string resource_manager()
{
    ::Display* display = tiny::get_display();
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long left = 0;
    unsigned char* data = nullptr;
    std::string text;
    if (XGetWindowProperty(display, RootWindow(display, 0), XA_RESOURCE_MANAGER,
                           0, 1L << 20, False, XA_STRING, &type, &format,
                           &count, &left, &data) == Success &&
        data != nullptr) {
        text.assign(reinterpret_cast<const char*>(data), count);
        XFree(data);
    }
    return text;
}

void set_resource_manager(const std::string& text)
{
    ::Display* display = tiny::get_display();
    XChangeProperty(display, RootWindow(display, 0), XA_RESOURCE_MANAGER,
                    XA_STRING, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(text.data()),
                    static_cast<int>(text.size()));
    XSync(display, False);
}

} // namespace

TEST(XResourcesTest, the_changes_go_to_the_property_of_the_root_window)
{
    const std::string before = resource_manager();
    set_resource_manager("*font: fixed\nXft.rgba: bgr\n");

    EXPECT_TRUE(tiny::wm::publish_resources(
            tiny::get_display(),
            {{"Xft.rgba", "rgb"}, {"Xft.antialias", "0"}}));
    EXPECT_EQ(resource_manager(),
              "*font: fixed\nXft.rgba:\trgb\nXft.antialias:\t0\n");
    // Xft of this connection asks Xlib, which has the old text unless told
    const char* rgba = XGetDefault(tiny::get_display(), "Xft", "rgba");
    ASSERT_NE(rgba, nullptr);
    EXPECT_STREQ(rgba, "rgb");

    set_resource_manager(before);
}

TEST(XResourcesTest, a_missing_property_starts_a_new_one)
{
    const std::string before = resource_manager();
    ::Display* display = tiny::get_display();
    XDeleteProperty(display, RootWindow(display, 0), XA_RESOURCE_MANAGER);

    EXPECT_TRUE(tiny::wm::publish_resources(display, {{"Xft.rgba", "rgb"}}));
    EXPECT_EQ(resource_manager(), "Xft.rgba:\trgb\n");

    set_resource_manager(before);
}
