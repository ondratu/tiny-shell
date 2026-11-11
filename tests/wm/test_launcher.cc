#include "wm_helpers.h"
#include "wm/shell_theme.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

#include "wm/launcher.h"
#include "wm/launcher_layout.h"

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
        launcher.set_local_share("/nonexistent"); // not the apps of the host
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

    ::Window grid() const { return children_of(launcher.get_window()).at(1); }

    ::Window prev_page() const
    {
        return children_of(launcher.get_window()).at(2);
    }

    ::Window next_page() const
    {
        return children_of(launcher.get_window()).at(3);
    }

    tiny::wm::LauncherLayout layout() const
    {
        const XWindowAttributes root_attrs = attributes_of(root_window());
        return tiny::wm::LauncherLayout::compute(
                root_attrs.width, root_attrs.height, tiny::wm::shell_theme);
    }

    size_t per_page() const
    {
        return static_cast<size_t>(layout().columns) *
               static_cast<size_t>(layout().rows);
    }

    static std::string app_id(size_t index)
    {
        const std::string number = std::to_string(index);
        return "app" +
               std::string(3 - std::min<size_t>(3, number.size()), '0') +
               number;
    }

    // More applications than a page, "App000" is the first
    void write_pages(size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            const std::string id = app_id(i);
            write(id, id, "Type=Application\n");
        }
    }

    // A click on a button, the pointer enters it first
    void click(::Window button)
    {
        send(button, make_event(EnterNotify));
        send(button, make_event(ButtonRelease));
    }

    bool shown() const
    {
        return attributes_of(launcher).map_state == IsViewable;
    }

    WidgetFont font;

    const ShellFonts shell_fonts;
    fs::path root;
    std::optional<EnvVar> data_home;
    EnvVar data_dirs{"XDG_DATA_DIRS", "/nonexistent"};
    EnvVar lang{"LC_ALL", "C"};
    tiny::wm::Launcher launcher;
};

TEST_F(LauncherTest, realize_covers_the_whole_screen)
{
    const XWindowAttributes root_attrs = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(launcher);

    EXPECT_EQ(attrs.x, 0);
    EXPECT_EQ(attrs.y, 0);
    EXPECT_EQ(attrs.border_width, 0);
    EXPECT_EQ(attrs.width, root_attrs.width);
    EXPECT_EQ(attrs.height, root_attrs.height);
    EXPECT_FALSE(shown());
}

TEST_F(LauncherTest, the_cells_are_centered_on_the_screen)
{
    const XWindowAttributes root_attrs = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(grid());

    EXPECT_EQ(attrs.x * 2 + attrs.width, root_attrs.width);
    EXPECT_EQ(attrs.y * 2 + layout().rows * layout().cell_height,
              root_attrs.height);
}

TEST_F(LauncherTest, the_entry_is_centered_between_the_panel_and_the_cells)
{
    const XWindowAttributes root_attrs = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(entry());
    const int border = tiny::theme.entry.border_width;

    EXPECT_EQ(attrs.x * 2 + attrs.width + 2 * border, root_attrs.width);
    EXPECT_GT(attrs.y, tiny::wm::shell_theme.panel);
    EXPECT_LT(attrs.y + attrs.height, attributes_of(grid()).y);
}

TEST_F(LauncherTest, popup_arranges_the_launcher_again)
{
    XMoveResizeWindow(tiny::get_display(), launcher.get_window(), 10, 10, 100,
                      100);

    launcher.popup();

    const XWindowAttributes root_attrs = attributes_of(root_window());
    const XWindowAttributes attrs = attributes_of(launcher);
    EXPECT_EQ(attrs.x, 0);
    EXPECT_EQ(attrs.y, 0);
    EXPECT_EQ(attrs.width, root_attrs.width);
    EXPECT_EQ(attrs.height, root_attrs.height);
}

TEST_F(LauncherTest, popup_keeps_the_given_windows_above_the_launcher)
{
    tiny::Widget panel(100, 30);
    panel.realize(root_window(), 0, 0);
    launcher.keep_above(&panel);

    launcher.popup();

    const auto windows = children_of(root_window());
    const auto at = [&](::Window w) {
        return std::find(windows.begin(), windows.end(), w) - windows.begin();
    };
    EXPECT_GT(at(panel.get_window()), at(launcher.get_window()));
}

TEST_F(LauncherTest, holds_an_entry_above_a_grid_and_two_buttons)
{
    ASSERT_EQ(children_of(launcher.get_window()).size(), 4u);
    EXPECT_LT(attributes_of(entry()).y, attributes_of(grid()).y);
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

TEST_F(LauncherTest, right_selects_the_next_application)
{
    launcher.popup();

    type_key(entry(), XK_Right);
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "beta"));
    EXPECT_FALSE(fs::exists(root / "alpha"));
}

TEST_F(LauncherTest, left_goes_back_to_the_previous_application)
{
    launcher.popup();

    type_key(entry(), XK_Right);
    type_key(entry(), XK_Left);
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "alpha"));
}

TEST_F(LauncherTest, the_keys_do_not_leave_the_grid)
{
    launcher.popup();

    type_key(entry(), XK_Left);
    type_key(entry(), XK_Up);
    type_key(entry(), XK_Down);
    type_key(entry(), XK_Page_Down);
    type_key(entry(), XK_Page_Up);
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "alpha"));
}

TEST_F(LauncherTest, click_on_a_cell_starts_the_application)
{
    launcher.popup();
    XEvent press = make_event(ButtonPress);
    press.xbutton.button = Button1;
    press.xbutton.x = layout().cell_width + 5; // the second cell
    press.xbutton.y = 5;
    XEvent release = press;
    release.type = ButtonRelease;

    send(grid(), press);
    send(grid(), release);

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

TEST_F(LauncherTest, a_terminal_application_runs_in_the_configured_terminal)
{
    write("alpha", "Alpha", "Type=Application\nTerminal=true\n");
    // the "terminal" leaves a mark and runs the command it is given
    launcher.set_terminal(
            {"sh", "-c",
             "touch '" + (root / "terminal").string() + "'; exec \"$@\"",
             "sh"});
    launcher.popup();

    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "terminal"));
    EXPECT_TRUE(appears(root / "alpha"));
}

TEST_F(LauncherTest, an_application_that_does_not_start_keeps_the_launcher)
{
    std::ofstream(root / "applications" / "alpha.desktop")
            << "[Desktop Entry]\nName=Alpha\nType=Application\n"
               "Exec=/nonexistent/program\n";
    launcher.popup();

    testing::internal::CaptureStderr();
    type_key(entry(), XK_Return);
    const std::string report = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(shown());
    EXPECT_NE(report.find("Alpha (/nonexistent/program)"), std::string::npos);
    EXPECT_NE(report.find("No such file"), std::string::npos);
}

TEST_F(LauncherTest, a_missing_terminal_is_the_program_that_is_reported)
{
    write("alpha", "Alpha", "Type=Application\nTerminal=true\n");
    launcher.set_terminal({"/nonexistent/terminal", "-e"});
    launcher.popup();

    testing::internal::CaptureStderr();
    type_key(entry(), XK_Return);
    const std::string report = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(shown());
    EXPECT_NE(report.find("Alpha (/nonexistent/terminal)"), std::string::npos);
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
    type_key(entry(), XK_Right);
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

TEST_F(LauncherTest, the_local_share_is_searched_when_the_variable_omits_it)
{
    const fs::path local = root / "local";
    fs::create_directories(local / "applications");
    std::ofstream(local / "applications" / "gamma.desktop")
            << "[Desktop Entry]\nName=Gamma\nType=Application\nExec=touch "
            << (root / "gamma").string() << "\n";
    launcher.set_local_share(local.string());

    launcher.popup();
    type_text(entry(), "gamma");
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "gamma"));
}

TEST_F(LauncherTest, the_application_of_a_window_gives_its_icon)
{
    fs::create_directories(root / "pixmaps");
    std::ofstream(root / "pixmaps" / "delta.xpm")
            << "/* XPM */\nstatic char * delta_xpm[] = {\n"
               "\"2 2 1 1\",\n\"a c #FF0000\",\n\"aa\",\n\"aa\"};\n";
    std::ofstream(root / "applications" / "cz.example.Delta.desktop")
            << "[Desktop Entry]\nName=Delta\nType=Application\nIcon=delta\n"
               "Exec=true\n";
    const EnvVar dirs("XDG_DATA_DIRS", root.string());

    const auto& icon = launcher.application_icon("delta", "Delta");
    EXPECT_NE(icon.picture, None);
    EXPECT_EQ(icon.width, tiny::wm::shell_theme.icon);
    EXPECT_EQ(&launcher.application_icon("delta", "Delta"), &icon); // cached

    EXPECT_EQ(launcher.application_icon("nothing", "Nothing").picture, None);
    EXPECT_EQ(launcher.application_icon("alpha", "Alpha").picture, None);
}

TEST_F(LauncherTest, the_icon_of_an_application_is_loaded_in_the_size_asked_for)
{
    fs::create_directories(root / "pixmaps");
    std::ofstream(root / "pixmaps" / "delta.xpm")
            << "/* XPM */\nstatic char * delta_xpm[] = {\n"
               "\"2 2 1 1\",\n\"a c #FF0000\",\n\"aa\",\n\"aa\"};\n";
    std::ofstream(root / "applications" / "cz.example.Delta.desktop")
            << "[Desktop Entry]\nName=Delta\nType=Application\nIcon=delta\n"
               "Exec=true\n";
    const EnvVar dirs("XDG_DATA_DIRS", root.string());
    const auto* app = launcher.find_application("cz.example.Delta.desktop");
    ASSERT_NE(app, nullptr);

    const auto& big = launcher.application_icon(*app, 128);

    EXPECT_NE(big.picture, None);
    EXPECT_EQ(&launcher.application_icon(*app, 128), &big); // cached
    EXPECT_NE(&launcher.application_icon(*app, 64), &big);  // one per size
    const auto* none = launcher.find_application("alpha.desktop");
    ASSERT_NE(none, nullptr);
    EXPECT_EQ(launcher.application_icon(*none, 128).picture, None);
}

TEST_F(LauncherTest,
       an_application_is_found_by_its_id_or_by_the_class_of_a_window)
{
    const auto* by_id = launcher.find_application("alpha.desktop");
    ASSERT_NE(by_id, nullptr);
    EXPECT_EQ(by_id->entry.name, "Alpha");

    EXPECT_EQ(launcher.find_application("alpha", "Alpha"), by_id);
    EXPECT_EQ(launcher.find_application("nothing.desktop"), nullptr);
    EXPECT_EQ(launcher.find_application("nothing", "Nothing"), nullptr);
}

TEST_F(LauncherTest, an_application_is_started_by_its_id)
{
    EXPECT_TRUE(launcher.launch("beta.desktop"));

    EXPECT_TRUE(appears(root / "beta"));
}

TEST_F(LauncherTest, an_application_that_is_not_installed_is_not_started)
{
    EXPECT_FALSE(launcher.launch("nothing.desktop"));
}

TEST_F(LauncherTest, an_application_that_does_not_start_is_told)
{
    std::ofstream(root / "applications" / "broken.desktop")
            << "[Desktop Entry]\nName=Broken\nType=Application\n"
               "Exec=/nonexistent/program\n";

    testing::internal::CaptureStderr();
    EXPECT_FALSE(launcher.launch("broken.desktop"));
    const std::string report = testing::internal::GetCapturedStderr();

    EXPECT_NE(report.find("Broken (/nonexistent/program)"), std::string::npos);
    EXPECT_NE(report.find("No such file"), std::string::npos);
}

TEST_F(LauncherTest,
       an_action_of_an_application_is_started_with_its_own_command)
{
    write("gamma", "Gamma",
          "Type=Application\nActions=one;two\n"
          "[Desktop Action one]\nName=One\nExec=touch " +
                  (root / "gamma-one").string() +
                  "\n[Desktop Action two]\nName=Two\nExec=touch " +
                  (root / "gamma-two").string() + "\n");

    EXPECT_EQ(launcher.action_names("gamma.desktop"),
              (std::vector<std::string>{"One", "Two"}));
    EXPECT_TRUE(launcher.launch_action("gamma.desktop", 1));

    EXPECT_TRUE(appears(root / "gamma-two"));
    EXPECT_FALSE(fs::exists(root / "gamma-one"));
    EXPECT_FALSE(fs::exists(root / "gamma"));
}

TEST_F(LauncherTest, there_is_no_action_of_an_unknown_application_or_number)
{
    EXPECT_TRUE(launcher.action_names("alpha.desktop").empty());
    EXPECT_TRUE(launcher.action_names("nothing.desktop").empty());
    EXPECT_FALSE(launcher.launch_action("alpha.desktop", 0));
    EXPECT_FALSE(launcher.launch_action("nothing.desktop", 0));
}

TEST_F(LauncherTest, the_right_button_on_an_application_tells_the_menu)
{
    launcher.popup();
    std::vector<std::string> ids;
    XButtonEvent where = {};
    launcher.on_menu = [&](const std::string& id, const XButtonEvent& be) {
        ids.push_back(id);
        where = be;
    };
    XEvent e = make_event(ButtonPress);
    e.xbutton.button = Button3;
    e.xbutton.x = 5;
    e.xbutton.y = 5;
    e.xbutton.x_root = 123;

    send(grid(), e); // the first cell is Alpha
    pump_events();

    EXPECT_EQ(ids, (std::vector<std::string>{"alpha.desktop"}));
    EXPECT_EQ(where.x_root, 123);
}

TEST_F(LauncherTest, the_right_button_without_a_handler_is_ignored)
{
    launcher.popup();
    XEvent e = make_event(ButtonPress);
    e.xbutton.button = Button3;
    e.xbutton.x = 5;
    e.xbutton.y = 5;

    send(grid(), e);

    EXPECT_TRUE(launcher.is_open());
}

TEST_F(LauncherTest, hide_closes_an_open_launcher_only)
{
    launcher.hide(); // closed already
    EXPECT_FALSE(launcher.is_open());

    launcher.popup();
    EXPECT_TRUE(launcher.is_open());
    launcher.hide();

    EXPECT_FALSE(launcher.is_open());
}

TEST_F(LauncherTest, refocus_gives_the_keyboard_to_the_search_field)
{
    launcher.popup();
    XSetInputFocus(tiny::get_display(), PointerRoot, RevertToNone, CurrentTime);

    launcher.refocus();

    ::Window focus;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_EQ(focus, entry());
}

TEST_F(LauncherTest, refocus_of_a_closed_launcher_does_nothing)
{
    XSetInputFocus(tiny::get_display(), PointerRoot, RevertToNone, CurrentTime);

    launcher.refocus();

    ::Window focus;
    int revert;
    XGetInputFocus(tiny::get_display(), &focus, &revert);
    EXPECT_NE(focus, entry());
}

TEST_F(LauncherTest, an_application_started_by_its_id_is_a_recent_one)
{
    ASSERT_TRUE(launcher.launch("beta.desktop"));
    ASSERT_TRUE(appears(root / "beta"));
    fs::remove(root / "beta");

    launcher.popup(); // Alpha sorts first, but Beta was used
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "beta"));
    EXPECT_FALSE(fs::exists(root / "alpha"));
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
    send(grid(), make_event(Expose));

    // the icon is above the text, both in the middle of the cell
    const int size = layout().icon;
    const XftFont* text_font = tiny::theme.list.get_font();
    const int content = size + tiny::theme.list.padding_width / 2 +
                        text_font->ascent + text_font->descent;
    const int top = (layout().cell_height - content) / 2;
    const Image image(grid(), attributes_of(grid()).width,
                      layout().cell_height);
    EXPECT_EQ(image.at(layout().cell_width / 2, top + size / 2), 0xFF0000u);
}

TEST_F(LauncherTest, an_application_with_no_icon_gets_the_generic_one)
{
    const IconFiles files({"application-x-executable"});
    tiny::wm::SymbolicIcons symbols(tiny::get_display(), root_window(),
                                    {files.dir()});
    launcher.set_icons(&symbols);

    launcher.popup();
    send(grid(), make_event(Expose));

    const int size = layout().icon;
    const XftFont* text_font = tiny::theme.list.get_font();
    const int content = size + tiny::theme.list.padding_width / 2 +
                        text_font->ascent + text_font->descent;
    const int top = (layout().cell_height - content) / 2;
    const Image image(grid(), attributes_of(grid()).width,
                      layout().cell_height);
    const auto normal = static_cast<uint8_t>(tiny::Style::State::Normal);
    EXPECT_EQ(image.at(layout().cell_width / 2, top + size / 2),
              tiny::wm::lighter(tiny::theme.list.get_fg(normal)));
}

TEST_F(LauncherTest, the_cells_take_at_most_the_three_quarters_of_the_screen)
{
    const XWindowAttributes root_attrs = attributes_of(root_window());

    EXPECT_LE(attributes_of(grid()).width, root_attrs.width * 3 / 4);
    EXPECT_LE(attributes_of(grid()).width,
              tiny::wm::shell_theme.launcher_columns * layout().cell_width);
}

TEST_F(LauncherTest, the_buttons_of_the_pages_are_hidden_for_one_page)
{
    launcher.popup();

    EXPECT_EQ(attributes_of(prev_page()).map_state, IsUnmapped);
    EXPECT_EQ(attributes_of(next_page()).map_state, IsUnmapped);
}

TEST_F(LauncherTest, the_buttons_of_the_pages_are_shown_for_more_pages)
{
    write_pages(per_page());

    launcher.popup();

    EXPECT_EQ(attributes_of(prev_page()).map_state, IsViewable);
    EXPECT_EQ(attributes_of(next_page()).map_state, IsViewable);
}

TEST_F(LauncherTest, the_buttons_follow_the_number_of_found_applications)
{
    write_pages(per_page());
    launcher.popup();

    type_text(entry(), "app000");

    EXPECT_EQ(attributes_of(next_page()).map_state, IsUnmapped);
}

TEST_F(LauncherTest, the_buttons_are_beside_the_cells)
{
    const XWindowAttributes cells = attributes_of(grid());

    EXPECT_LT(attributes_of(prev_page()).x + attributes_of(prev_page()).width,
              cells.x);
    EXPECT_GT(attributes_of(next_page()).x, cells.x + cells.width);
}

TEST_F(LauncherTest, the_button_turns_to_the_next_page)
{
    const size_t page = per_page();
    write_pages(2 * page);
    launcher.popup();
    type_text(entry(), "app");

    click(next_page());
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / app_id(page)));
    EXPECT_FALSE(fs::exists(root / "app000"));
}

TEST_F(LauncherTest, the_button_turns_back_to_the_previous_page)
{
    write_pages(2 * per_page());
    launcher.popup();
    type_text(entry(), "app");

    click(next_page());
    click(prev_page());
    type_key(entry(), XK_Return);

    EXPECT_TRUE(appears(root / "app000"));
}
