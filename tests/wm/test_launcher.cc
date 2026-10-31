#include "helpers.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

#include "wm/launcher.h"

namespace fs = std::filesystem;

namespace {

// Waits (up to 2 s) for the started program to create `path`.
bool appears(const fs::path& path)
{
    for (int i = 0; i < 200; ++i) {
        if (fs::exists(path)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

} // namespace

class LauncherTest : public ::testing::Test {
  protected:
    void SetUp() override
    {
        root = fs::temp_directory_path() /
               ("tiny-launcher-" + std::to_string(getpid()));
        fs::remove_all(root);
        fs::create_directories(root / "applications");
        // "Alpha" sorts before "Beta"; both start a program that creates a file
        write("alpha", "Alpha", "Type=Application\nKeywords=first;\n");
        write("beta", "Beta", "Type=Application\n");
        write("hidden", "Hidden", "Type=Application\nNoDisplay=true\n");
        data_home.emplace("XDG_DATA_HOME", root.string());
        launcher.realize(root_window());
    }

    void TearDown() override { fs::remove_all(root); }

    void write(const std::string& id, const std::string& name,
               const std::string& more)
    {
        std::ofstream(root / "applications" / (id + ".desktop"))
                << "[Desktop Entry]\nName=" << name << "\nExec=touch "
                << (root / id).string() << "\n"
                << more;
    }

    ::Window entry() const { return children_of(launcher.get_window()).at(0); }

    ::Window list() const { return children_of(launcher.get_window()).at(1); }

    bool shown() const
    {
        return attributes_of(launcher).map_state == IsViewable;
    }

    WidgetFont font;
    fs::path root;
    std::optional<EnvVar> data_home;
    EnvVar data_dirs{"XDG_DATA_DIRS", "/nonexistent"};
    EnvVar lang{"LC_ALL", "C"};
    tiny::wm::Launcher launcher;
};

TEST_F(LauncherTest, realize_centers_the_launcher_horizontally)
{
    const XWindowAttributes root_attrs = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(launcher);

    EXPECT_EQ(attrs.width, tiny::theme.wm_launcher_width);
    EXPECT_EQ(attrs.x, root_attrs.width / 2 - attrs.width / 2);
    EXPECT_GE(attrs.y, 0);
    EXPECT_FALSE(shown());
}

TEST_F(LauncherTest, popup_centers_the_launcher_again)
{
    XMoveWindow(tiny::get_display(), launcher.get_window(), 0, 0);

    launcher.popup();

    const XWindowAttributes root_attrs = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(launcher);
    EXPECT_EQ(attrs.x, root_attrs.width / 2 - attrs.width / 2);
}

TEST_F(LauncherTest, holds_an_entry_above_a_list)
{
    ASSERT_EQ(children_of(launcher.get_window()).size(), 2u);
    EXPECT_LT(attributes_of(entry()).y, attributes_of(list()).y);
}

TEST_F(LauncherTest, popup_shows_the_launcher_and_focuses_the_entry)
{
    launcher.popup();
    XSync(tiny::get_display(), False);

    EXPECT_TRUE(shown());
    ::Window focus = None;
    int revert = 0;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_EQ(focus, entry());
}

TEST_F(LauncherTest, toggle_shows_and_hides_the_launcher)
{
    launcher.toggle();
    EXPECT_TRUE(shown());

    launcher.toggle();
    EXPECT_FALSE(shown());
}

TEST_F(LauncherTest, return_starts_the_first_application)
{
    launcher.popup();

    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "alpha"));
    EXPECT_FALSE(shown());
}

TEST_F(LauncherTest, hidden_applications_are_not_listed)
{
    launcher.popup();
    type_text(entry(), "hid");

    type_key(entry(), XK_Return);

    EXPECT_TRUE(shown()); // nothing found, nothing started
    EXPECT_FALSE(fs::exists(root / "hidden"));
}

TEST_F(LauncherTest, typing_searches_the_applications)
{
    launcher.popup();
    type_text(entry(), "bet");

    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "beta"));
    EXPECT_FALSE(fs::exists(root / "alpha"));
}

TEST_F(LauncherTest, keywords_are_searched)
{
    launcher.popup();
    type_text(entry(), "first");

    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "alpha"));
}

TEST_F(LauncherTest, down_selects_the_next_application)
{
    launcher.popup();

    type_key(entry(), XK_Down);
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "beta"));
    EXPECT_FALSE(fs::exists(root / "alpha"));
}

TEST_F(LauncherTest, up_and_the_pages_do_not_leave_the_list)
{
    launcher.popup();

    type_key(entry(), XK_Up);
    type_key(entry(), XK_Page_Down);
    type_key(entry(), XK_Page_Up);
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "alpha"));
}

TEST_F(LauncherTest, click_on_a_row_starts_the_application)
{
    launcher.popup();
    XEvent press = make_event(ButtonPress);
    press.xbutton.button = Button1;
    press.xbutton.y = tiny::theme.wm_launcher_icon +
                      2 * tiny::theme.list.padding_width; // the second row
    XEvent release = press;
    release.type = ButtonRelease;

    send(list(), press);
    send(list(), release);

    EXPECT_TRUE(appears(root / "beta"));
    EXPECT_FALSE(shown());
}

TEST_F(LauncherTest, nothing_found_keeps_the_launcher_open)
{
    launcher.popup();
    type_text(entry(), "zzz");

    type_key(entry(), XK_Return);

    EXPECT_TRUE(shown());
}

TEST_F(LauncherTest, an_application_that_does_not_start_keeps_the_launcher)
{
    std::ofstream(root / "applications" / "alpha.desktop")
            << "[Desktop Entry]\nName=Alpha\nType=Application\n"
               "Exec=/nonexistent/program\n";
    launcher.popup();

    type_key(entry(), XK_Return);

    EXPECT_TRUE(shown());
}

TEST_F(LauncherTest, escape_closes_the_launcher_and_forgets_the_search)
{
    launcher.popup();
    type_text(entry(), "bet");

    type_key(entry(), XK_Escape);
    launcher.popup();
    type_key(entry(), XK_Return);

    EXPECT_FALSE(shown()); // closed by Return, so Alpha was found again
    EXPECT_TRUE(appears(root / "alpha"));
}

TEST_F(LauncherTest, the_started_application_comes_first_next_time)
{
    launcher.popup();
    type_key(entry(), XK_Down);
    type_key(entry(), XK_Return);
    ASSERT_TRUE(appears(root / "beta"));
    fs::remove(root / "beta");

    launcher.popup();
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "beta"));
    EXPECT_FALSE(fs::exists(root / "alpha"));
}

TEST_F(LauncherTest, applications_are_read_when_the_launcher_opens_first)
{
    fs::remove(root / "applications" / "alpha.desktop");

    launcher.popup();
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "beta"));
}

TEST_F(LauncherTest, an_icon_of_the_application_is_drawn)
{
    fs::create_directories(root / "pixmaps");
    std::ofstream(root / "pixmaps" / "alpha.xpm")
            << "/* XPM */\nstatic char * alpha_xpm[] = {\n"
               "\"2 2 1 1\",\n\"a c #FF0000\",\n\"aa\",\n\"aa\"};\n";
    std::ofstream(root / "applications" / "alpha.desktop")
            << "[Desktop Entry]\nName=Alpha\nType=Application\nIcon=alpha\n"
               "Exec=true\n";
    const EnvVar dirs("XDG_DATA_DIRS", root.string());

    launcher.popup();
    send(list(), make_event(Expose));

    const int size = tiny::theme.wm_launcher_icon;
    const int padding = tiny::theme.list.padding_width;
    const Image image(list(), attributes_of(list()).width, 2 * size);
    EXPECT_EQ(image.at(padding + size / 2, padding + size / 2), 0xFF0000u);
}
