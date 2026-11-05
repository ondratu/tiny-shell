#include "wm_helpers.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "wm/symbolic_icons.h"

namespace fs = std::filesystem;
using tiny::wm::SymbolicIcons;

class SymbolicIconsTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-symbolic-" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "tiny-shell/icons/white");
        std::ofstream(root / "tiny-shell/icons/white/start-here-symbolic.svg")
                << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" "
                   "height=\"16\" viewBox=\"0 0 16 16\"><path fill=\"#ffffff\" "
                   "d=\"M0 0H16V16H0Z\"/></svg>\n";
    }

    void TearDown() override { fs::remove_all(root); }

    ::Display* display() const { return tiny::get_display(); }

    fs::path root;
};

TEST_F(SymbolicIconsTest, finds_the_file_in_the_data_directories)
{
    SymbolicIcons icons(display(), root_window(),
                        {"/nonexistent", root.string()});

    EXPECT_EQ(
            icons.find("start-here"),
            (root / "tiny-shell/icons/white/start-here-symbolic.svg").string());
    EXPECT_EQ(icons.find("window-close"), "");
}

TEST_F(SymbolicIconsTest, makes_the_icon_in_the_size_once)
{
    SymbolicIcons icons(display(), root_window(), {root.string()});

    const SymbolicIcons::Icon& icon = icons.get("start-here", 24, 0xFF0000);

    EXPECT_NE(icon.picture, None);
    EXPECT_EQ(icon.width, 24);
    EXPECT_EQ(icon.height, 24);
    EXPECT_EQ(&icons.get("start-here", 24, 0xFF0000), &icon);
    EXPECT_NE(&icons.get("start-here", 24, 0x00FF00), &icon);
}

TEST_F(SymbolicIconsTest, a_missing_icon_is_empty_and_is_not_drawn)
{
    SymbolicIcons icons(display(), root_window(), {root.string()});

    EXPECT_EQ(icons.get("window-close", 16, 0).picture, None);
    EXPECT_FALSE(icons.draw(root_window(), "window-close", 16, 0, 20, 20));
}

TEST_F(SymbolicIconsTest, draws_in_the_middle_in_the_color)
{
    SymbolicIcons icons(display(), root_window(), {root.string()});
    ::Display* dpy = display();
    const ::Window w = XCreateSimpleWindow(dpy, root_window(), 0, 0, 20, 20, 0,
                                           0, 0x000000);
    XMapWindow(dpy, w);

    EXPECT_TRUE(icons.draw(w, "start-here", 10, 0xFF0000, 20, 20));

    const Image img(w, 20, 20);
    EXPECT_EQ(img.at(10, 10), 0xFF0000u);
    EXPECT_EQ(img.at(1, 1), 0x000000u); // outside of the icon
    XDestroyWindow(dpy, w);
}
