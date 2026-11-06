#include "wm_helpers.h"

#include "wm/monitor_label.h"

using tiny::wm::Monitor;
using tiny::wm::MonitorLabel;
using tiny::wm::MonitorLabels;

namespace {

class MonitorLabelTest : public ::testing::Test {
  protected:
    HeaderFont header_font;
    WidgetFont widget_font;
};

} // namespace

TEST_F(MonitorLabelTest, the_detail_has_the_output_and_the_size)
{
    const Monitor monitor = {"HDMI-1", {1920, 0, 1280, 1024}, false};

    EXPECT_EQ(MonitorLabels::detail_of(monitor, false), "HDMI-1 1280x1024");
    EXPECT_EQ(MonitorLabels::detail_of(monitor, true),
              "HDMI-1 1280x1024 (primary)");
}

TEST_F(MonitorLabelTest, a_monitor_without_a_name_is_the_screen)
{
    const Monitor monitor = {"", {0, 0, 800, 600}, true};

    EXPECT_EQ(MonitorLabels::detail_of(monitor, false), "screen 800x600");
}

TEST_F(MonitorLabelTest, a_label_is_in_the_corner_of_its_monitor)
{
    MonitorLabel label;
    label.realize(root_window(), 0, 0);
    label.set_events();
    label.set_text("2", "DP-1 1920x1080");

    label.place({1920, 100, 1920, 1080});
    XSync(tiny::get_display(), False);

    const XWindowAttributes a = attributes_of(label);
    EXPECT_GT(a.x, 1920);
    EXPECT_LT(a.x, 1920 + 400);
    EXPECT_GT(a.y, 100);
    EXPECT_GT(a.width, 20);
    EXPECT_GT(a.height, 20);
    EXPECT_EQ(label.get_number(), "2");
    EXPECT_EQ(label.get_detail(), "DP-1 1920x1080");
}

TEST_F(MonitorLabelTest, there_is_a_label_for_every_monitor_over_the_windows)
{
    MonitorLabels labels;
    const tiny::wm::Monitors monitors = {{"A", {0, 0, 640, 480}, false},
                                         {"B", {640, 0, 640, 480}, true}};
    EXPECT_FALSE(labels.is_shown());

    labels.show(root_window(), monitors, 1);

    ASSERT_TRUE(labels.is_shown());
    ASSERT_EQ(labels.get_labels().size(), 2u);
    EXPECT_EQ(labels.get_labels()[0]->get_number(), "1");
    EXPECT_EQ(labels.get_labels()[1]->get_number(), "2");
    EXPECT_EQ(labels.get_labels()[1]->get_detail(), "B 640x480 (primary)");
    EXPECT_EQ(attributes_of(*labels.get_labels()[0]).map_state, IsViewable);
    EXPECT_TRUE(attributes_of(*labels.get_labels()[0]).override_redirect);
    EXPECT_GE(attributes_of(*labels.get_labels()[1]).x, 640);
}

TEST_F(MonitorLabelTest, the_labels_follow_the_number_of_monitors)
{
    MonitorLabels labels;
    labels.show(
            root_window(),
            {{"A", {0, 0, 640, 480}, false}, {"B", {640, 0, 640, 480}, false}},
            0);

    labels.show(root_window(), {{"A", {0, 0, 640, 480}, true}}, 0);
    EXPECT_EQ(labels.get_labels().size(), 1u);

    labels.hide();
    EXPECT_FALSE(labels.is_shown());
}
