#include "wm_helpers.h"

#include "wm/monitors.h"

using namespace tiny::wm;

namespace {

Monitor monitor(const char* name, int x, int y, int w, int h,
                bool primary = false)
{
    return {name, {x, y, w, h}, primary};
}

Monitors two()
{
    return {monitor("DP-1", 0, 0, 1920, 1080),
            monitor("HDMI-1", 1920, 0, 1280, 1024, true)};
}

} // namespace

TEST(MonitorsTest, are_numbered_from_the_left_and_then_from_the_top)
{
    Monitors list = {monitor("c", 1920, 1080, 800, 600),
                     monitor("b", 1920, 0, 800, 600),
                     monitor("a", 0, 0, 1920, 1080)};

    order_monitors(list);

    ASSERT_EQ(list.size(), 3u);
    EXPECT_EQ(list[0].name, "a");
    EXPECT_EQ(list[1].name, "b");
    EXPECT_EQ(list[2].name, "c");
}

TEST(MonitorsTest, a_clone_is_not_a_monitor_of_its_own)
{
    Monitors list = {monitor("a", 0, 0, 1920, 1080),
                     monitor("clone", 0, 0, 1024, 768, true)};

    order_monitors(list);

    ASSERT_EQ(list.size(), 1u);
    EXPECT_EQ(list[0].name, "a");
    EXPECT_TRUE(list[0].primary); // the clone of the primary one
}

TEST(MonitorsTest, the_whole_screen_around_two_monitors_is_not_a_monitor)
{
    Monitors list = {monitor("A", 0, 0, 640, 800),
                     monitor("B", 640, 0, 640, 800),
                     monitor("screen", 0, 0, 1280, 800)};

    order_monitors(list);

    ASSERT_EQ(list.size(), 2u);
    EXPECT_EQ(list[0].name, "A");
    EXPECT_EQ(list[1].name, "B");
}

TEST(MonitorsTest, the_screen_has_at_least_one)
{
    const Monitors list = read_monitors(tiny::get_display(), root_window());

    ASSERT_GE(list.size(), 1u);
    EXPECT_GT(list[0].area.width, 0);
    EXPECT_GT(list[0].area.height, 0);
}

TEST(MonitorsTest, are_found_by_number_or_by_name)
{
    const Monitors list = two();

    EXPECT_EQ(find_monitor(list, "1"), std::optional<size_t>(0));
    EXPECT_EQ(find_monitor(list, " 2 "), std::optional<size_t>(1));
    EXPECT_EQ(find_monitor(list, "hdmi-1"), std::optional<size_t>(1));
    EXPECT_FALSE(find_monitor(list, "0"));
    EXPECT_FALSE(find_monitor(list, "3"));
    EXPECT_FALSE(find_monitor(list, "99999999999"));
    EXPECT_FALSE(find_monitor(list, "VGA-1"));
    EXPECT_FALSE(find_monitor(list, ""));
    EXPECT_FALSE(find_monitor(list, "auto"));
}

TEST(MonitorsTest, a_setting_has_no_blanks_inside)
{
    EXPECT_TRUE(valid_monitor_spec("auto"));
    EXPECT_TRUE(valid_monitor_spec("DP-1"));
    EXPECT_TRUE(valid_monitor_spec(""));
    EXPECT_FALSE(valid_monitor_spec("DP 1"));
    EXPECT_FALSE(valid_monitor_spec("DP\n1"));
}

TEST(MonitorsTest, auto_primary_is_the_one_of_randr)
{
    const MonitorChoice choice = choose_monitors(two(), "auto", "auto");

    EXPECT_EQ(choice.primary, 1u);
    EXPECT_EQ(choice.secondary, std::optional<size_t>(0));
}

TEST(MonitorsTest, auto_primary_without_the_flag_is_the_first)
{
    Monitors list = two();
    list[1].primary = false;

    const MonitorChoice choice = choose_monitors(list, "", "");

    EXPECT_EQ(choice.primary, 0u);
    EXPECT_EQ(choice.secondary, std::optional<size_t>(1));
}

TEST(MonitorsTest, the_settings_choose_the_monitors)
{
    Monitors list = two();
    list.push_back(monitor("VGA-1", 3200, 0, 800, 600));

    const MonitorChoice choice = choose_monitors(list, "1", "VGA-1");

    EXPECT_EQ(choice.primary, 0u);
    EXPECT_EQ(choice.secondary, std::optional<size_t>(2));
}

TEST(MonitorsTest, the_secondary_is_never_the_primary)
{
    const MonitorChoice choice = choose_monitors(two(), "1", "1");

    EXPECT_EQ(choice.primary, 0u);
    EXPECT_EQ(choice.secondary, std::optional<size_t>(1));
}

TEST(MonitorsTest, one_monitor_has_no_secondary)
{
    const Monitors list = {monitor("DP-1", 0, 0, 800, 600)};

    const MonitorChoice choice = choose_monitors(list, "2", "3");

    EXPECT_EQ(choice.primary, 0u);
    EXPECT_FALSE(choice.secondary);
}

TEST(MonitorsTest, placements_have_names)
{
    for (const Placement placement : placements()) {
        EXPECT_EQ(parse_placement(placement_name(placement)),
                  std::optional<Placement>(placement));
    }
    EXPECT_EQ(parse_placement(""), std::optional<Placement>(Placement::Keep));
    EXPECT_EQ(parse_placement(" Right "),
              std::optional<Placement>(Placement::Right));
    EXPECT_FALSE(parse_placement("diagonal"));
}

TEST(MonitorsTest, knows_where_a_monitor_is)
{
    const tiny::Rect primary = {1000, 500, 800, 600};

    EXPECT_TRUE(is_placed({1800, 0, 100, 100}, primary, Placement::Right));
    EXPECT_FALSE(is_placed({1000, 0, 100, 100}, primary, Placement::Right));
    EXPECT_TRUE(is_placed({900, 0, 100, 100}, primary, Placement::Left));
    EXPECT_FALSE(is_placed({800, 0, 100, 100}, primary, Placement::Left));
    EXPECT_TRUE(is_placed({0, 400, 100, 100}, primary, Placement::Over));
    EXPECT_FALSE(is_placed({0, 300, 100, 100}, primary, Placement::Over));
    EXPECT_TRUE(is_placed({0, 1100, 100, 100}, primary, Placement::Under));
    EXPECT_FALSE(is_placed({0, 1000, 100, 100}, primary, Placement::Under));
    EXPECT_TRUE(is_placed({0, 0, 1, 1}, primary, Placement::Keep));
}

TEST(MonitorsTest, the_placement_is_one_xrandr_command)
{
    const Monitors list = two(); // DP-1 is on the left of HDMI-1

    const auto swap = placement_command(list[0], list[1], Placement::Right);
    EXPECT_EQ(swap, (std::vector<std::string>{"xrandr", "--output", "DP-1",
                                              "--right-of", "HDMI-1"}));
    EXPECT_EQ(placement_command(list[0], list[1], Placement::Left).size(),
              0u); // it is there
    EXPECT_EQ(placement_command(list[0], list[1], Placement::Over)[3],
              "--above");
    EXPECT_EQ(placement_command(list[0], list[1], Placement::Under)[3],
              "--below");
    EXPECT_EQ(placement_command(list[1], list[0], Placement::Left)[3],
              "--left-of");
}

TEST(MonitorsTest, nothing_is_run_when_there_is_nothing_to_do)
{
    Monitors list = two();

    EXPECT_TRUE(placement_command(list[0], list[1], Placement::Keep).empty());
    list[0].name.clear(); // no RandR: there is no output to move
    EXPECT_TRUE(placement_command(list[0], list[1], Placement::Right).empty());
    EXPECT_TRUE(placement_command(list[1], list[1], Placement::Right).empty());
}

TEST(MonitorsTest, a_frame_belongs_to_the_monitor_with_most_of_it)
{
    const Monitors list = two();

    EXPECT_EQ(monitor_of(list, {100, 100, 400, 300}), 0u);
    EXPECT_EQ(monitor_of(list, {2000, 100, 400, 300}), 1u);
    EXPECT_EQ(monitor_of(list, {1800, 100, 400, 300}), 1u); // 280 of 400 wide
    EXPECT_EQ(monitor_of(list, {1700, 100, 400, 300}), 0u);
}

TEST(MonitorsTest, a_frame_outside_belongs_to_the_nearest_monitor)
{
    const Monitors list = two();

    EXPECT_EQ(monitor_of(list, {-900, 100, 100, 100}), 0u);
    EXPECT_EQ(monitor_of(list, {5000, 0, 100, 100}), 1u);
    EXPECT_EQ(monitor_at(list, 1919, 5), 0u);
    EXPECT_EQ(monitor_at(list, 1920, 5), 1u);
    EXPECT_EQ(monitor_at(list, 2000, 1100), 1u);
}

TEST(MonitorsTest, a_window_is_centered_over_an_area)
{
    const tiny::Rect r = center_in({1920, 30, 1280, 994}, 400, 200);

    EXPECT_EQ(r.x, 1920 + 440);
    EXPECT_EQ(r.y, 30 + 397);
    EXPECT_EQ(r.width, 400);
    EXPECT_EQ(r.height, 200);
    // bigger than the area: starts at its corner
    EXPECT_EQ(center_in({10, 20, 100, 100}, 300, 300).x, 10);
    EXPECT_EQ(center_in({10, 20, 100, 100}, 300, 300).y, 20);
}

TEST(MonitorsTest, the_events_of_randr_are_recognized_only_with_it)
{
    const std::optional<int> base =
            select_monitor_events(tiny::get_display(), root_window());

    EXPECT_EQ(base.has_value(), has_randr());
    EXPECT_FALSE(is_monitor_event(make_event(Expose), base.value_or(100)));
}
