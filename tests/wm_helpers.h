#pragma once

// The helpers of the tests of the shell; the ones of the widgets are in
// helpers.h (the tests of the widgets must not need the shell)
#include "helpers.h"
#include "wm/shell_theme.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>

// The screen below the panel, the work area without any dock.
inline tiny::Rect screen_area()
{
    XWindowAttributes a;
    XGetWindowAttributes(tiny::get_display(), root_window(), &a);
    return {0, tiny::wm::shell_theme.panel, a.width,
            a.height - tiny::wm::shell_theme.panel};
}

// Same as WidgetFont for wm::shell_theme.header.
class HeaderFont {
  public:
    HeaderFont(): original(tiny::wm::shell_theme.header), with_font(original)
    {
        with_font.init();
        tiny::wm::shell_theme.header = with_font;
    }

    ~HeaderFont() { tiny::wm::shell_theme.header = original; }

  private:
    tiny::wm::HeaderStyle original;
    tiny::wm::HeaderStyle with_font;
};

// Gives the styles of the panel, the dock and the launcher a font for the lifetime of the
// object (they draw text with their own), like WidgetFont does for the toolkit.
class ShellFonts {
  public:
    ShellFonts():
        original_panel(tiny::wm::shell_theme.panel_widgets),
        panel_with_font(original_panel),
        original_dock(tiny::wm::shell_theme.dock_widgets),
        dock_with_font(original_dock),
        original_launcher(tiny::wm::shell_theme.launcher_widgets),
        launcher_with_font(original_launcher),
        original_list(tiny::wm::shell_theme.dock_list),
        list_with_font(original_list)
    {
        panel_with_font.init();
        tiny::wm::shell_theme.panel_widgets = panel_with_font;
        dock_with_font.init();
        tiny::wm::shell_theme.dock_widgets = dock_with_font;
        launcher_with_font.init();
        tiny::wm::shell_theme.launcher_widgets = launcher_with_font;
        list_with_font.init();
        tiny::wm::shell_theme.dock_list = list_with_font;
    }

    ~ShellFonts()
    {
        tiny::wm::shell_theme.panel_widgets = original_panel;
        tiny::wm::shell_theme.dock_widgets = original_dock;
        tiny::wm::shell_theme.launcher_widgets = original_launcher;
        tiny::wm::shell_theme.dock_list = original_list;
    }

  private:
    tiny::Style original_panel;
    tiny::Style panel_with_font;
    tiny::Style original_dock;
    tiny::Style dock_with_font;
    tiny::Style original_launcher;
    tiny::Style launcher_with_font;
    tiny::ListStyle original_list;
    tiny::ListStyle list_with_font;
};

// A data directory (the layout of share/) with a full white square as every
// symbolic icon that is asked for: the place of a square shows the icon.
class IconFiles {
  public:
    explicit IconFiles(std::initializer_list<const char*> names):
        root(std::filesystem::temp_directory_path() /
             ("tiny-icons-" + std::to_string(getpid()) + "-" +
              std::to_string(counter++)))
    {
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root / "tiny-shell/icons/white");
        for (const char* name : names) {
            std::ofstream(root / "tiny-shell/icons/white" /
                          (std::string(name) + "-symbolic.svg"))
                    << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
                       "width=\"16\" height=\"16\" viewBox=\"0 0 16 16\">"
                       "<path fill=\"#ffffff\" d=\"M0 0H16V16H0Z\"/></svg>\n";
        }
    }

    IconFiles(const IconFiles&) = delete;
    IconFiles& operator=(const IconFiles&) = delete;

    ~IconFiles() { std::filesystem::remove_all(root); }

    std::string dir() const { return root.string(); }

  private:
    static inline int counter = 0;
    std::filesystem::path root;
};
