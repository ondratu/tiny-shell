#include "fake_client.h"
#include "wm_helpers.h"

#include <X11/Xatom.h>

#include <cstring>
#include <map>

#include "wm/xsettings.h"

using tiny::wm::XSetting;
using tiny::wm::XSettings;

namespace {

uint32_t u32(const std::vector<unsigned char>& bytes, size_t at)
{
    uint32_t value = 0;
    std::memcpy(&value, bytes.data() + at, 4);
    return value;
}

//! What a program that reads XSETTINGS sees (the byte order is ours)
struct Decoded {
    uint32_t serial = 0;
    std::map<std::string, XSetting> settings;
};

Decoded decode(const std::vector<unsigned char>& bytes)
{
    Decoded rv;
    rv.serial = u32(bytes, 4);
    const uint32_t count = u32(bytes, 8);
    size_t at = 12;
    for (uint32_t i = 0; i < count; ++i) {
        const auto type = static_cast<XSetting::Type>(bytes[at]);
        uint16_t length = 0;
        std::memcpy(&length, bytes.data() + at + 2, 2);
        at += 4;
        const std::string name(bytes.begin() + static_cast<long>(at),
                               bytes.begin() + static_cast<long>(at + length));
        at += (length + 3u) & ~3u;
        XSetting setting;
        setting.type = type;
        setting.name = name;
        setting.changed = u32(bytes, at);
        at += 4;
        if (type == XSetting::Type::Integer) {
            setting.number = static_cast<int>(u32(bytes, at));
            at += 4;
        } else {
            const uint32_t size = u32(bytes, at);
            at += 4;
            setting.text.assign(bytes.begin() + static_cast<long>(at),
                                bytes.begin() + static_cast<long>(at + size));
            at += (size + 3u) & ~3u;
        }
        rv.settings[name] = setting;
    }
    EXPECT_EQ(at, bytes.size());
    return rv;
}

//! The property of the owner of the selection, as a program reads it
std::vector<unsigned char> property_of(::Window owner)
{
    ::Display* display = tiny::get_display();
    const Atom property = XInternAtom(display, "_XSETTINGS_SETTINGS", False);
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long left = 0;
    unsigned char* data = nullptr;
    std::vector<unsigned char> rv;
    if (XGetWindowProperty(display, owner, property, 0, 1 << 16, False,
                           property, &type, &format, &count, &left,
                           &data) == Success &&
        data != nullptr) {
        EXPECT_EQ(type, property);
        EXPECT_EQ(format, 8);
        rv.assign(data, data + count);
        XFree(data);
    }
    return rv;
}

} // namespace

TEST(XSettingsEncodeTest,
     the_header_has_the_byte_order_the_serial_and_the_count)
{
    const auto bytes = tiny::wm::encode_xsettings({}, 7);

    ASSERT_EQ(bytes.size(), 12u);
    const uint16_t one = 1;
    unsigned char first = 0;
    std::memcpy(&first, &one, 1);
    EXPECT_EQ(bytes[0], first == 0 ? 1 : 0); // MSBFirst is 1
    EXPECT_EQ(u32(bytes, 4), 7u);
    EXPECT_EQ(u32(bytes, 8), 0u);
}

TEST(XSettingsEncodeTest, a_number_and_a_text_are_padded_to_four_bytes)
{
    XSetting number = XSetting::integer("Xft/DPI", 98304);
    number.changed = 3;
    XSetting text = XSetting::string("Net/ThemeName", "Adwaita");
    text.changed = 5;

    const auto bytes = tiny::wm::encode_xsettings({number, text}, 5);

    // 12 header + (4 + "Xft/DPI" 7 -> 8 + 4 + 4) + (4 + "Net/ThemeName" 13 ->
    // 16 + 4 + 4 + "Adwaita" 7 -> 8)
    EXPECT_EQ(bytes.size(), 12u + 20u + 36u);
    EXPECT_EQ(bytes.size() % 4, 0u);
    const Decoded decoded = decode(bytes);
    EXPECT_EQ(decoded.serial, 5u);
    EXPECT_EQ(decoded.settings.at("Xft/DPI").number, 98304);
    EXPECT_EQ(decoded.settings.at("Xft/DPI").changed, 3u);
    EXPECT_EQ(decoded.settings.at("Net/ThemeName").text, "Adwaita");
}

TEST(XSettingsEncodeTest, an_empty_text_and_a_negative_number_survive)
{
    const auto bytes = tiny::wm::encode_xsettings(
            {XSetting::string("A", ""), XSetting::integer("B", -1)}, 1);

    const Decoded decoded = decode(bytes);
    EXPECT_EQ(decoded.settings.at("A").text, "");
    EXPECT_EQ(decoded.settings.at("B").number, -1);
}

TEST(XSettingsOfTest, the_theme_name_follows_the_variant)
{
    const tiny::wm::Config config;

    const auto dark = tiny::wm::xsettings_of(config, tiny::Variant::Dark);
    const auto light = tiny::wm::xsettings_of(config, tiny::Variant::Light);

    const auto theme = [](const std::vector<XSetting>& list) {
        for (const XSetting& s : list) {
            if (s.name == "Net/ThemeName") {
                return s.text;
            }
        }
        return std::string("none");
    };
    EXPECT_EQ(theme(dark), "Adwaita-dark");
    EXPECT_EQ(theme(light), "Adwaita");
}

namespace {

std::map<std::string, XSetting> by_name(const std::vector<XSetting>& list)
{
    std::map<std::string, XSetting> rv;
    for (const XSetting& s : list) {
        rv[s.name] = s;
    }
    return rv;
}

} // namespace

TEST(XSettingsOfTest, the_font_is_the_first_family_the_style_and_the_size)
{
    tiny::wm::Config config;
    config.font = "Noto Sans,sans";
    config.font_size = 10.5;

    auto settings =
            by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Gtk/FontName").text, "Noto Sans 10.5");

    config.font_style = "Bold";
    config.font_size = 12;
    settings = by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Gtk/FontName").text, "Noto Sans Bold 12");
}

TEST(XSettingsOfTest, the_dpi_follows_the_scale)
{
    tiny::wm::Config config;

    auto settings =
            by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Xft/DPI").number, 96 * 1024);
    EXPECT_EQ(settings.at("Gdk/UnscaledDPI").number, 96 * 1024);
    EXPECT_EQ(settings.at("Gdk/WindowScalingFactor").number, 1);

    config.scale = 1.5; // the windows are not scaled, the text is
    settings = by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Xft/DPI").number, 144 * 1024);
    EXPECT_EQ(settings.at("Gdk/UnscaledDPI").number, 144 * 1024);
    EXPECT_EQ(settings.at("Gdk/WindowScalingFactor").number, 1);

    config.scale = 2.5; // 2 for the windows, the rest for the text
    settings = by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Xft/DPI").number, 240 * 1024);
    EXPECT_EQ(settings.at("Gdk/UnscaledDPI").number, 120 * 1024);
    EXPECT_EQ(settings.at("Gdk/WindowScalingFactor").number, 2);

    config.scale = 0.5; // never below one for the windows
    settings = by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Xft/DPI").number, 48 * 1024);
    EXPECT_EQ(settings.at("Gdk/WindowScalingFactor").number, 1);
}

TEST(XSettingsOfTest, what_xft_does_not_set_is_left_out)
{
    tiny::wm::Config config;

    auto settings =
            by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    for (const char* name :
         {"Xft/Antialias", "Xft/Hinting", "Xft/HintStyle", "Xft/RGBA"}) {
        EXPECT_EQ(settings.count(name), 0u) << name;
    }

    config.xft_antialias = "no";
    config.xft_hinting = "slight";
    config.xft_rgba = "vrgb";
    settings = by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Xft/Antialias").number, 0);
    EXPECT_EQ(settings.at("Xft/Hinting").number, 1);
    EXPECT_EQ(settings.at("Xft/HintStyle").text, "hintslight");
    EXPECT_EQ(settings.at("Xft/RGBA").text, "vrgb");

    config.xft_antialias = "yes";
    config.xft_hinting = "none";
    settings = by_name(tiny::wm::xsettings_of(config, tiny::Variant::Dark));
    EXPECT_EQ(settings.at("Xft/Antialias").number, 1);
    EXPECT_EQ(settings.at("Xft/Hinting").number, 0);
    EXPECT_EQ(settings.at("Xft/HintStyle").text, "hintnone");
}

TEST(XSettingsOfTest, the_settings_are_sorted_by_name)
{
    tiny::wm::Config config;
    config.xft_hinting = "full";

    const auto list = tiny::wm::xsettings_of(config, tiny::Variant::Dark);

    for (size_t i = 1; i < list.size(); ++i) {
        EXPECT_LT(list[i - 1].name, list[i].name);
    }
}

class XSettingsTest : public ::testing::Test {
  protected:
    ::Display* display = tiny::get_display();
    ::Window root = root_window();
    int screen = XDefaultScreen(display);
    Atom selection = XInternAtom(display, "_XSETTINGS_S0", False);
};

TEST_F(XSettingsTest, the_owner_has_the_selection_and_the_property)
{
    XSettings owner(display, root, screen);
    owner.set({XSetting::integer("Xft/DPI", 98304)});

    ASSERT_TRUE(owner.acquire());

    EXPECT_EQ(XGetSelectionOwner(display, selection), owner.get_window());
    const Decoded decoded = decode(property_of(owner.get_window()));
    EXPECT_EQ(decoded.settings.at("Xft/DPI").number, 98304);
}

TEST_F(XSettingsTest, the_programs_are_told_with_the_manager_message)
{
    const FakeClient program;
    XSelectInput(program.display, root, StructureNotifyMask);
    program.sync();
    XSettings owner(display, root, screen);

    ASSERT_TRUE(owner.acquire());
    program.sync();

    bool told = false;
    XEvent e;
    while (XCheckTypedEvent(program.display, ClientMessage, &e)) {
        told = told ||
               (e.xclient.message_type ==
                        XInternAtom(program.display, "MANAGER", False) &&
                static_cast<Atom>(e.xclient.data.l[1]) == selection);
    }
    EXPECT_TRUE(told);
}

TEST_F(XSettingsTest, a_change_is_published_with_a_new_serial)
{
    XSettings owner(display, root, screen);
    owner.set({XSetting::integer("A", 1), XSetting::integer("B", 1)});
    ASSERT_TRUE(owner.acquire());
    const uint32_t first = owner.get_serial();

    owner.set({XSetting::integer("A", 1), XSetting::integer("B", 2)});

    EXPECT_GT(owner.get_serial(), first);
    const Decoded decoded = decode(property_of(owner.get_window()));
    EXPECT_EQ(decoded.serial, owner.get_serial());
    EXPECT_EQ(decoded.settings.at("B").number, 2);
    // only the setting that changed has the new serial
    EXPECT_EQ(decoded.settings.at("B").changed, owner.get_serial());
    EXPECT_EQ(decoded.settings.at("A").changed, first);
}

TEST_F(XSettingsTest, the_same_settings_change_nothing)
{
    XSettings owner(display, root, screen);
    owner.set({XSetting::string("A", "x")});
    ASSERT_TRUE(owner.acquire());
    const uint32_t serial = owner.get_serial();

    owner.set({XSetting::string("A", "x")});

    EXPECT_EQ(owner.get_serial(), serial);
}

TEST_F(XSettingsTest, a_setting_that_goes_away_or_comes_is_a_change)
{
    XSettings owner(display, root, screen);
    owner.set({XSetting::integer("A", 1)});
    ASSERT_TRUE(owner.acquire());
    const uint32_t serial = owner.get_serial();

    owner.set({XSetting::integer("A", 1), XSetting::integer("B", 1)});
    EXPECT_GT(owner.get_serial(), serial);
    EXPECT_EQ(decode(property_of(owner.get_window())).settings.size(), 2u);

    owner.set({XSetting::integer("B", 1)});
    EXPECT_EQ(decode(property_of(owner.get_window())).settings.size(), 1u);
}

TEST_F(XSettingsTest, another_manager_of_settings_is_not_replaced)
{
    FakeClient daemon;
    XSetSelectionOwner(daemon.display, selection, daemon.window, CurrentTime);
    daemon.sync();
    XSettings owner(display, root, screen);

    EXPECT_FALSE(owner.acquire());

    EXPECT_EQ(XGetSelectionOwner(display, selection), daemon.window);
}

TEST_F(XSettingsTest, the_selection_is_lost_when_somebody_takes_it)
{
    XSettings owner(display, root, screen);
    ASSERT_TRUE(owner.acquire());
    XSelectInput(display, owner.get_window(), PropertyChangeMask);
    const FakeClient daemon;
    XSetSelectionOwner(daemon.display, selection, daemon.window, CurrentTime);
    daemon.sync();

    XEvent e;
    ASSERT_TRUE(XCheckTypedEvent(display, SelectionClear, &e));
    EXPECT_TRUE(owner.is_lost_by(e.xselectionclear));
}
