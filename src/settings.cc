// tiny-settings: changes the font, the terminal, the scale and the rendering
// of text of tiny-shell.
//
// The form has a panel at the left with the sections: Fonts (the font, its
// size and style, and the antialiasing, hinting and subpixel order of the text
// of the programs), Appearance (the scale, the wallpaper, the mode of the dock
// and the format of the clock), Applications (the terminal) and Screens (the
// primary and the secondary monitor, where the secondary one is, and the
// numbers on the monitors). The choices
// go through their values with the arrows, Space and a click; a field has a
// cursor (the arrows, Home, End, Delete and the pointer move it). Ctrl+Page
// Down and Ctrl+Page Up go through the sections. The button next to the font opens a list of the fonts of
// the system (type to search it) with the size and the style of the font and
// a preview of a text in them. Save writes the values to the configuration
// file of the user (never the global one) and closes the program; tiny-shell
// reads the file when it starts, so a change is seen after the next login. The
// rendering of text is also put into the X resources (Xft.*) at once, so the
// programs that start later use it. Escape and Cancel close the program
// without writing.
//
// The text is drawn by Xft into a pixmap that is copied to the window, so the
// window does not flicker. What can be decided without X (the fields, the
// list, the keys) is in SettingsForm.

#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "clipboard.h"
#include "display.h"
#include "spawn.h"
#include "entry.h"
#include "app/settings_form.h"
#include "app/wallpaper.h"
#include "text.h"
#include "text_field.h"
#include "style.h"
#include "wm/config.h"
#include "wm/xresources.h"
#include "x_util.h"
#include "xdg/application_index.h"
#include "xdg/font_list.h"
#include "xft_window.h"

namespace {

using tiny::app::SettingsForm;

constexpr const char* title = "Settings";

//! More fonts than this are closed and opened again when they are needed
constexpr size_t font_cache_limit = 64;

//! The initial height of the window, at the scale 1
constexpr int start_height = 600;
constexpr int start_width = 560;

constexpr const char* hint =
        "Applies after login; the text options in new programs at once.";

constexpr tiny::wm::XftOption xft_options[] = {tiny::wm::XftOption::Antialias,
                                               tiny::wm::XftOption::Hinting,
                                               tiny::wm::XftOption::Rgba};

//! The value of an option of `[xft]` in the form
const std::string& xft_value(const SettingsForm::Values& v,
                             tiny::wm::XftOption option)
{
    switch (option) {
        case tiny::wm::XftOption::Antialias:
            return v.antialias;
        case tiny::wm::XftOption::Hinting:
            return v.hinting;
        case tiny::wm::XftOption::Rgba:
            break;
    }
    return v.rgba;
}

class SettingsApp : public tiny::XftWindow {
  public:
    SettingsApp(tiny::Display& display_arg, const tiny::wm::Config& config,
                std::string file_arg, std::string wallpaper_program_arg,
                std::vector<std::string> families):
        XftWindow(display_arg),
        file(std::move(file_arg)),
        wallpaper_program(std::move(wallpaper_program_arg)),
        scale(config.scale),
        metrics(SettingsForm::Metrics::scaled(config.scale)),
        initial{config.font,
                config.terminal_line,
                tiny::number_text(config.scale),
                tiny::number_text(config.font_size),
                config.font_style,
                config.xft_antialias,
                config.xft_hinting,
                config.xft_rgba,
                tiny::wm::wallpaper_of(config, config.style,
                                       tiny::xdg::default_data_dirs()),
                config.wallpaper_mode,
                config.clock_format,
                config.dock_mode == tiny::wm::DockMode::AutoHide
                        ? ""
                        : tiny::wm::dock_mode_name(config.dock_mode),
                config.monitor_primary == "auto" ? "" : config.monitor_primary,
                config.monitor_secondary == "auto" ? ""
                                                   : config.monitor_secondary,
                config.monitor_placement == tiny::wm::Placement::Keep
                        ? ""
                        : tiny::wm::placement_name(config.monitor_placement),
                config.monitor_numbers ? "yes" : ""},
        form(initial, std::move(families), metrics, tiny::xdg::font_styles),
        ui_family(config.font),
        background(alloc_color(0xf8, 0xf8, 0xf8)),
        sidebar(alloc_color(0xed, 0xed, 0xed)),
        field(alloc_color(0xff, 0xff, 0xff)),
        text_color(alloc_color(0x2e, 0x34, 0x36)),
        dim(alloc_color(0x6e, 0x74, 0x76)),
        rule(alloc_color(0xc8, 0xc8, 0xc8)),
        accent(alloc_color(0x72, 0x9f, 0xcf)),
        selected(alloc_color(0xdc, 0xe8, 0xf5)),
        error(alloc_color(0xc0, 0x1c, 0x28))
    {
        // the font of the shell, when it can be opened, else any sans
        ui_font = text_font(ui_family);
        if (ui_font == nullptr) {
            ui_family = "sans";
            ui_font = text_font(ui_family);
        }

        form.set_measure([this](const std::string& text) {
            return text_width(ui_font, text);
        });

        const int w = std::min(tiny::scaled(start_width, scale),
                               DisplayWidth(display, screen));
        const int h = std::min(
                std::max(tiny::scaled(start_height, scale), form.form_height()),
                DisplayHeight(display, screen));
        form.resize(w, h);
        create({title, "tiny-settings", "TinySettings", w, h,
                tiny::scaled(start_width, scale) * 3 / 4, form.form_height(), 0,
                true});
    }

  protected:
    void on_resize(int w, int h) override { form.resize(w, h); }

    void on_button_release(const XButtonEvent& e) override
    {
        if (e.button == Button1 && form.release(e.x, e.y)) {
            act();
        }
    }

    void on_key(XKeyEvent& e) override
    {
        KeySym symbol = NoSymbol;
        const std::string typed = lookup(e, symbol);
        if ((e.state & ControlMask) != 0) {
            if (symbol == XK_q || symbol == XK_w) {
                quit();
            } else if (symbol == XK_v || symbol == XK_V) {
                paste(tiny::Clipboard::Which::Clipboard, e.time);
            } else if ((symbol == XK_c || symbol == XK_C) &&
                       form.view() == SettingsForm::View::Form &&
                       !SettingsForm::is_choice(form.focus()) &&
                       !form.text(form.focus()).empty()) {
                // there is no selection in a field: the whole text is copied;
                // nothing to copy leaves the clipboard as it is
                clipboard().own(tiny::Clipboard::Which::Clipboard,
                                form.text(form.focus()), e.time);
            } else if (symbol == XK_Page_Down || symbol == XK_Page_Up ||
                       symbol == XK_Tab || symbol == XK_ISO_Left_Tab) {
                const bool back = symbol == XK_Page_Up ||
                                  symbol == XK_ISO_Left_Tab ||
                                  (e.state & ShiftMask) != 0;
                if (form.key(back ? SettingsForm::Key::PrevSection
                                  : SettingsForm::Key::NextSection)) {
                    act();
                }
            }
            return;
        }
        if (symbol == XK_Insert && (e.state & ShiftMask) != 0) {
            paste(tiny::Clipboard::Which::Clipboard, e.time);
            return;
        }
        static const std::pair<KeySym, SettingsForm::Key> keys[] = {
                {XK_Up, SettingsForm::Key::Up},
                {XK_Down, SettingsForm::Key::Down},
                {XK_Left, SettingsForm::Key::Left},
                {XK_Right, SettingsForm::Key::Right},
                {XK_Delete, SettingsForm::Key::Delete},
                {XK_KP_Delete, SettingsForm::Key::Delete},
                {XK_Page_Up, SettingsForm::Key::PageUp},
                {XK_Page_Down, SettingsForm::Key::PageDown},
                {XK_Home, SettingsForm::Key::Home},
                {XK_End, SettingsForm::Key::End},
                {XK_Return, SettingsForm::Key::Return},
                {XK_KP_Enter, SettingsForm::Key::Return},
                {XK_BackSpace, SettingsForm::Key::Back},
                {XK_Escape, SettingsForm::Key::Escape},
                {XK_Tab, SettingsForm::Key::Tab},
                {XK_ISO_Left_Tab, SettingsForm::Key::BackTab}};
        for (const auto& [code, key] : keys) {
            if (symbol == code) {
                const bool back =
                        key == SettingsForm::Key::Tab && (e.state & ShiftMask);
                if (form.key(back ? SettingsForm::Key::BackTab : key)) {
                    act();
                }
                return;
            }
        }
        if (form.type(typed)) {
            act();
        }
    }

    void on_button_press(const XButtonEvent& e) override
    {
        if (e.button == Button1) {
            form.press(e.x, e.y);
        } else if (e.button == Button2) {
            paste(tiny::Clipboard::Which::Primary, e.time);
        } else if ((e.button == Button4 || e.button == Button5) &&
                   form.wheel(e.button == Button4 ? -1 : 1)) {
            act();
        }
    }

    //! Type the text of a selection into the focused field (one line of it)
    void paste(tiny::Clipboard::Which which, Time time)
    {
        clipboard().request(
                which,
                [this](const std::string& text) {
                    if (form.type(tiny::single_line(text))) {
                        update();
                    }
                },
                time);
    }

    //! Paint after something changed, and act on what the user asked for
    void act()
    {
        switch (form.result()) {
            case SettingsForm::Result::Editing:
                break;
            case SettingsForm::Result::Cancelled:
                quit();
                return;
            case SettingsForm::Result::Saved:
                if (save()) {
                    quit();
                    return;
                }
                break;
        }
        update();
    }

    //! Write the file of the user; on a failure the form tells why
    bool save()
    {
        std::string why;
        if (file.empty()) {
            why = "Neither XDG_CONFIG_HOME nor HOME is set.";
        } else {
            // only what the user changed: the values that come from the global
            // file stay there, so a later change of that file is still seen
            const SettingsForm::Values v = form.clean_values();
            std::vector<tiny::wm::IniValue> changed;
            if (v.font != initial.font) {
                changed.push_back({"theme", "font", v.font});
            }
            if (v.scale != initial.scale) {
                changed.push_back({"theme", "scale", v.scale});
            }
            if (v.size != initial.size) {
                changed.push_back({"theme", "font_size", v.size});
            }
            if (v.style != initial.style) {
                changed.push_back({"theme", "font_style", v.style});
            }
            for (const tiny::wm::XftOption option : xft_options) {
                if (xft_value(v, option) != xft_value(initial, option)) {
                    changed.push_back({"xft", tiny::wm::xft_key(option),
                                       xft_value(v, option)});
                }
            }
            if (v.terminal != initial.terminal) {
                changed.push_back({"commands", "terminal", v.terminal});
            }
            const bool wallpaper_changed =
                    v.wallpaper != initial.wallpaper ||
                    v.wallpaper_mode != initial.wallpaper_mode;
            if (v.wallpaper != initial.wallpaper) {
                changed.push_back({"wallpaper", "file", v.wallpaper});
            }
            if (v.wallpaper_mode != initial.wallpaper_mode) {
                changed.push_back({"wallpaper", "mode", v.wallpaper_mode});
            }
            if (v.clock_format != initial.clock_format) {
                changed.push_back({"panel", "clock_format", v.clock_format});
            }
            if (v.dock != initial.dock) {
                // the first choice, "", is auto-hide
                changed.push_back(
                        {"dock", "mode", v.dock.empty() ? "autohide" : v.dock});
            }
            if (v.primary_monitor != initial.primary_monitor) {
                changed.push_back({"monitors", "primary",
                                   v.primary_monitor.empty()
                                           ? "auto"
                                           : v.primary_monitor});
            }
            if (v.secondary_monitor != initial.secondary_monitor) {
                changed.push_back({"monitors", "secondary",
                                   v.secondary_monitor.empty()
                                           ? "auto"
                                           : v.secondary_monitor});
            }
            if (v.monitor_placement != initial.monitor_placement) {
                changed.push_back({"monitors", "placement",
                                   v.monitor_placement.empty()
                                           ? "keep"
                                           : v.monitor_placement});
            }
            if (v.monitor_numbers != initial.monitor_numbers) {
                changed.push_back({"monitors", "numbers",
                                   v.monitor_numbers.empty() ? "no" : "yes"});
            }
            if (changed.empty() || tiny::wm::save_values(file, changed, &why)) {
                publish_xft(v);
                // a wallpaper that is removed goes at the next login: the
                // color of the theme belongs to tiny-shell
                if (wallpaper_changed && !v.wallpaper.empty() &&
                    tiny::spawn({wallpaper_program}) < 0) {
                    fprintf(stderr, "%s: can't run %s\n", "tiny-settings",
                            wallpaper_program.c_str());
                }
                return true;
            }
        }
        form.reject("Not saved: " + why);
        return false;
    }

    //! Put the changed rendering of text into the X resources, where the
    //! programs that start later find it; a failure is not worth stopping
    //! for, the file has the values and tiny-shell publishes them at login
    void publish_xft(const SettingsForm::Values& v)
    {
        std::vector<tiny::wm::ResourceChange> changes;
        for (const tiny::wm::XftOption option : xft_options) {
            if (xft_value(v, option) != xft_value(initial, option)) {
                for (auto& c :
                     tiny::wm::xft_resources(option, xft_value(v, option))) {
                    changes.push_back(std::move(c));
                }
            }
        }
        if (!changes.empty() &&
            !tiny::wm::publish_resources(display, changes)) {
            fprintf(stderr, "tiny-settings: can't change RESOURCE_MANAGER\n");
        }
    }

    XftFont* text_font(const std::string& family)
    {
        return font(family, metrics.text_size);
    }

    //! A frame of one pixel (two at a high scale) inside of the rectangle
    void frame(const XftColor& color, const tiny::Rect& r)
    {
        XftWindow::frame(color, r, std::max(1, metrics.row_height / 34));
    }

    //! A field with its text, and the cursor when it has the focus
    void paint_field(const tiny::Rect& r, const tiny::TextField& edited,
                     bool focused, bool wrong)
    {
        const int padding = metrics.padding;
        fill(field, r);
        const XftColor* border = focused ? &accent : &rule;
        if (wrong) {
            border = &error;
        }
        frame(*border, r);
        const int shift = form.shift(edited, r.width - 2 * padding);
        text_in(ui_font, text_color, r, padding, edited.text(), shift);
        if (focused && ui_font != nullptr) {
            const int before = text_width(
                    ui_font, edited.text().substr(0, edited.cursor()));
            const int line = std::max(1, metrics.row_height / 34);
            fill(text_color,
                 {r.x + padding + shift + before, r.y + metrics.row_height / 5,
                  line, metrics.row_height * 3 / 5});
        }
    }

    void paint() override
    {
        fill(background, {0, 0, width, height});
        if (form.view() == SettingsForm::View::Picker) {
            paint_picker();
        } else {
            paint_form();
        }
    }

    void paint_button(const tiny::Rect& r, const std::string& label,
                      bool enabled)
    {
        fill(field, r);
        frame(enabled ? accent : rule, r);
        text_in(ui_font, enabled ? text_color : dim, r,
                std::max(0, (r.width - text_width(ui_font, label)) / 2), label);
    }

    //! The two rows of the message: what is wrong, then the hint
    void paint_message(const std::string& wrong, const char* note)
    {
        const tiny::Rect message = form.message_rect();
        const tiny::Rect first = {message.x, message.y, message.width,
                                  message.height / 2};
        const tiny::Rect second = {message.x, message.y + message.height / 2,
                                   message.width, message.height / 2};
        text_in(ui_font, error, first, 0, fit(ui_font, wrong, message.width));
        text_in(ui_font, dim, second, 0, fit(ui_font, note, message.width));
    }

    //! A choice: the name of its value, an arrow at each end (a click on the
    //! left half goes back, on the right half goes on)
    void paint_choice(const tiny::Rect& r, const std::string& label,
                      bool focused)
    {
        fill(field, r);
        frame(focused ? accent : rule, r);
        const std::string back = "\xE2\x80\xB9";
        const std::string next = "\xE2\x80\xBA";
        const int padding = metrics.padding;
        text_in(ui_font, dim, r, padding, back);
        text_in(ui_font, text_color, r,
                padding + text_width(ui_font, back) + padding, label);
        text_in(ui_font, dim, r,
                std::max(0, r.width - padding - text_width(ui_font, next)),
                next);
    }

    //! The panel with the sections, the one that shows is marked
    void paint_sidebar()
    {
        fill(sidebar, form.sidebar_rect());
        fill(rule, {form.sidebar_rect().width - 1, 0, 1, height});
        for (size_t i = 0; i < SettingsForm::section_count; ++i) {
            const auto section = static_cast<SettingsForm::Section>(i);
            const tiny::Rect r = form.section_rect(section);
            if (section == form.section()) {
                fill(selected, r);
                fill(accent,
                     {r.x, r.y, std::max(1, metrics.row_height / 8), r.height});
            }
            text_in(ui_font, text_color, r, metrics.margin,
                    SettingsForm::section_title(section));
        }
    }

    //! The small picture of the wallpaper, so the user sees that the path is
    //! right; it is read when the field is left (not at every key: a photo
    //! takes a moment to decode)
    void refresh_thumbnail()
    {
        const std::string path(
                tiny::trim(form.text(SettingsForm::Field::Wallpaper)));
        const tiny::Rect r = form.thumbnail_rect();
        if (path == thumb_path && r.width == thumb_box.width &&
            r.height == thumb_box.height) {
            return;
        }
        thumb_path = path;
        thumb_box = r;
        thumb.reset();
        if (!path.empty()) {
            thumb = tiny::app::wallpaper_thumbnail(path, getenv("HOME"),
                                                   r.width, r.height);
        }
    }

    void paint_thumbnail()
    {
        if (form.focus() != SettingsForm::Field::Wallpaper) {
            refresh_thumbnail();
        }
        const tiny::Rect r = form.thumbnail_rect();
        fill(field, r);
        frame(rule, r);
        const bool current =
                thumb_path ==
                tiny::trim(form.text(SettingsForm::Field::Wallpaper));
        if (thumb && current) {
            put_picture(*thumb, r);
        } else {
            std::string why = "Leave the field to see the picture";
            if (form.text(SettingsForm::Field::Wallpaper).empty()) {
                why = "No wallpaper";
            } else if (current) {
                why = "Not a PNG or JPEG that can be read";
            }
            text_in(ui_font, dim, r, metrics.padding, why);
        }
    }

    //! Draw an opaque picture in the middle of a rectangle
    void put_picture(const tiny::image::Image& picture, const tiny::Rect& r)
    {
        Visual* visual = DefaultVisual(display, screen);
        if (visual->c_class != TrueColor ||
            DefaultDepth(display, screen) < 24) {
            return;
        }
        XImage* image = XCreateImage(
                display, visual,
                static_cast<unsigned>(DefaultDepth(display, screen)), ZPixmap,
                0, nullptr, static_cast<unsigned>(picture.width),
                static_cast<unsigned>(picture.height), 32, 0);
        if (image == nullptr) {
            return;
        }
        std::vector<uint32_t> pixels = picture.pixels; // 0xAARRGGBB
        image->data = reinterpret_cast<char*>(pixels.data());
        XPutImage(display, buffer, DefaultGC(display, screen), image, 0, 0,
                  r.x + (r.width - picture.width) / 2,
                  r.y + (r.height - picture.height) / 2,
                  static_cast<unsigned>(picture.width),
                  static_cast<unsigned>(picture.height));
        image->data = nullptr; // the vector owns it
        XDestroyImage(image);
    }

    void paint_form()
    {
        paint_sidebar();
        if (form.section() == SettingsForm::Section::Appearance) {
            paint_thumbnail();
        }
        static const struct {
            SettingsForm::Field id;
            const char* label;
        } rows[] = {{SettingsForm::Field::Font, "Font"},
                    {SettingsForm::Field::Size, "Size"},
                    {SettingsForm::Field::Style, "Style"},
                    {SettingsForm::Field::Terminal, "Terminal"},
                    {SettingsForm::Field::Scale, "Scale"},
                    {SettingsForm::Field::Antialias, "Antialias"},
                    {SettingsForm::Field::Hinting, "Hinting"},
                    {SettingsForm::Field::Rgba, "Subpixel"},
                    {SettingsForm::Field::Wallpaper, "Wallpaper"},
                    {SettingsForm::Field::WallpaperMode, "Placement"},
                    {SettingsForm::Field::ClockFormat, "Clock"},
                    {SettingsForm::Field::Dock, "Dock"},
                    {SettingsForm::Field::PrimaryMonitor, "Primary"},
                    {SettingsForm::Field::SecondaryMonitor, "Secondary"},
                    {SettingsForm::Field::MonitorPlacement, "Position"},
                    {SettingsForm::Field::MonitorNumbers, "Numbers"}};
        for (const auto& row : rows) {
            if (SettingsForm::section_of(row.id) != form.section()) {
                continue;
            }
            text_in(ui_font, text_color, form.label_rect(row.id), 0, row.label);
            const tiny::Rect r = form.field_rect(row.id);
            if (SettingsForm::is_choice(row.id)) {
                paint_choice(r, form.choice_label(row.id),
                             form.focus() == row.id);
                continue;
            }
            paint_field(r, form.field(row.id), form.focus() == row.id,
                        !form.problem(row.id).empty());
        }
        if (form.section() == SettingsForm::Section::Fonts) {
            paint_button(form.choose_rect(), "Choose…", true);
        }

        std::string wrong = form.problem();
        if (wrong.empty()) {
            wrong = form.status();
        }
        const char* note = SettingsForm::hint(form.focus());
        paint_message(wrong, *note != '\0' ? note : hint);

        paint_button(form.cancel_rect(), "Cancel", true);
        paint_button(form.save_rect(), "Save", form.problem().empty());
    }

    void paint_preview()
    {
        const tiny::Rect r = form.preview_rect();
        fill(field, r);
        frame(rule, r);
        if (!form.pick_problem().empty()) {
            text_in(ui_font, error, r, metrics.padding, form.pick_problem());
            return;
        }
        if (form.preview_family().empty()) {
            return;
        }
        // the size is the one of the shell, so it follows the scale
        XftFont* f = font_by_name(tiny::xft_name(
                form.preview_family(), form.preview_size() * scale,
                tiny::xft_style(form.pick_style())));
        text_in(f != nullptr ? f : ui_font, text_color, r, metrics.padding,
                SettingsForm::preview_text());
    }

    void paint_picker()
    {
        // the fonts of the rows that show are never thrown away mid-paint
        if (cached_fonts() > font_cache_limit + 2 * form.visible_rows()) {
            close_fonts();
            ui_font = text_font(ui_family);
        }
        const int padding = metrics.padding;
        const tiny::Rect search = form.search_rect();
        const bool in_search = form.pane() == SettingsForm::Pane::Search;
        if (form.query().empty() && !in_search) {
            fill(field, search);
            frame(rule, search);
            text_in(ui_font, dim, search, padding, "Type to search the fonts");
        } else {
            paint_field(search, form.search_field(), in_search, false);
        }

        if (form.families().empty()) {
            const tiny::Rect none = form.row_rect(0);
            text_in(ui_font, dim, none, padding, "No font matches");
        }
        const size_t end = std::min(form.families().size(),
                                    form.first() + form.visible_rows());
        for (size_t i = form.first(); i < end; ++i) {
            const tiny::Rect row = form.row_rect(i);
            const std::string& family = form.families()[i];
            if (i == form.selected()) {
                fill(selected, row);
            }
            // every family in its own font; the name in the font of the form
            // when the family has no glyphs for it
            XftFont* f = text_font(family);
            text_in(f != nullptr ? f : ui_font, text_color, row, metrics.margin,
                    family);
        }

        paint_preview();

        text_in(ui_font, text_color, form.size_label_rect(), 0, "Size");
        paint_field(form.size_rect(), form.size_field(),
                    form.pane() == SettingsForm::Pane::Size,
                    !form.pick_problem().empty());
        text_in(ui_font, text_color, form.style_label_rect(), 0, "Style");
        const tiny::Rect style = form.style_rect();
        fill(field, style);
        frame(form.pane() == SettingsForm::Pane::Style ? accent : rule, style);
        const std::string label =
                form.pick_style().empty() ? "Default" : form.pick_style();
        text_in(ui_font, text_color, style, padding,
                "\xE2\x80\xB9  " + label + "  \xE2\x80\xBA");

        paint_button(form.cancel_rect(), "Cancel", true);
        paint_button(form.save_rect(), "Select", form.pick_problem().empty());
    }

    std::string thumb_path;              //!< what `thumb` is the picture of
    tiny::Rect thumb_box = {0, 0, 0, 0}; //!< and the box it was made for
    std::optional<tiny::image::Image> thumb;
    std::string file; //!< of the user, empty when its place is not known
    std::string wallpaper_program; //!< draws the wallpaper after it is saved
    double scale;
    SettingsForm::Metrics metrics;
    SettingsForm::Values initial; //!< what the files say now
    SettingsForm form;
    std::string ui_family; //!< the family of `ui_font`
    XftFont* ui_font = nullptr;
    XftColor& background;
    XftColor& sidebar;
    XftColor& field;
    XftColor& text_color;
    XftColor& dim;
    XftColor& rule;
    XftColor& accent;
    XftColor& selected;
    XftColor& error;
};

} // namespace

int main(int argc, char** argv)
{
    if (argc > 1) {
        const bool help = std::strcmp(argv[1], "--help") == 0;
        fprintf(help ? stdout : stderr,
                "usage: %s\nChanges the font (its size and style), the "
                "terminal, the scale of tiny-shell, the rendering of text "
                "(antialiasing, hinting, subpixel order) and the wallpaper "
                "(the path of a PNG or JPEG and how it covers the screen).\n"
                "The settings are written to the configuration file of the "
                "user and are used\nafter the next login; the rendering of "
                "text is also used by the programs that start later and the "
                "wallpaper is drawn at once.\n",
                argv[0]);
        return help ? EXIT_SUCCESS : 2;
    }
    // the characters only: the numbers of the file always have a decimal point
    setlocale(LC_CTYPE, "");

    // the values in use now: the files of the system and of the user together
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
        SettingsApp app(tiny::get_display(), config,
                        tiny::wm::user_config_file(getenv("XDG_CONFIG_HOME"),
                                                   getenv("HOME")),
                        tiny::sibling_program(argv[0], "tiny-wallpaper"),
                        tiny::xdg::font_families());
        app.run();
    } catch (const std::exception& e) {
        fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
