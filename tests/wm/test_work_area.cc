#include "fake_client.h"
#include "wm_helpers.h"

#include "wm/work_area.h"

using tiny::wm::compute_work_area;
using tiny::wm::fit_in_area;
using tiny::wm::read_struts;
using tiny::wm::Struts;

TEST(WorkAreaTest, without_struts_the_area_is_the_screen)
{
    const tiny::Rect a = compute_work_area(1024, 768, {});

    EXPECT_EQ(a.x, 0);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.width, 1024);
    EXPECT_EQ(a.height, 768);
}

TEST(WorkAreaTest, the_panel_is_a_strut_like_a_dock)
{
    const tiny::Rect a = compute_work_area(1024, 768, {Struts{0, 0, 30, 0}});

    EXPECT_EQ(a.y, 30);
    EXPECT_EQ(a.height, 738);
}

TEST(WorkAreaTest, docks_reserve_the_edges)
{
    const tiny::Rect a = compute_work_area(
            1000, 800,
            {Struts{50, 0, 0, 0}, Struts{0, 20, 0, 0}, Struts{0, 0, 0, 40}});

    EXPECT_EQ(a.x, 50);
    EXPECT_EQ(a.y, 0);
    EXPECT_EQ(a.width, 930);
    EXPECT_EQ(a.height, 760);
}

TEST(WorkAreaTest, the_biggest_strut_of_an_edge_wins)
{
    const tiny::Rect a = compute_work_area(
            1000, 800, {Struts{0, 0, 10, 0}, Struts{0, 0, 60, 0}});

    EXPECT_EQ(a.y, 60);
    EXPECT_EQ(a.height, 740);
}

TEST(WorkAreaTest, docks_cannot_take_the_whole_screen)
{
    const tiny::Rect a =
            compute_work_area(100, 100, {Struts{500, 500, 500, 500}});

    EXPECT_GE(a.width, 1);
    EXPECT_GE(a.height, 1);
    EXPECT_LE(a.x + a.width, 100);
    EXPECT_LE(a.y + a.height, 100);
}

TEST(FitInAreaTest, a_frame_inside_stays)
{
    const tiny::Rect r = fit_in_area({100, 100, 200, 150}, {0, 30, 1000, 700});

    EXPECT_EQ(r.x, 100);
    EXPECT_EQ(r.y, 100);
    EXPECT_EQ(r.width, 200);
    EXPECT_EQ(r.height, 150);
}

TEST(FitInAreaTest, a_frame_is_moved_inside)
{
    const tiny::Rect r = fit_in_area({-20, 0, 200, 150}, {0, 30, 1000, 700});
    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 30);

    const tiny::Rect s = fit_in_area({900, 650, 200, 150}, {0, 30, 1000, 700});
    EXPECT_EQ(s.x, 800);
    EXPECT_EQ(s.y, 580);
}

TEST(FitInAreaTest, a_big_frame_is_made_smaller)
{
    const tiny::Rect r = fit_in_area({10, 10, 2428, 1428}, {0, 30, 1024, 738});

    EXPECT_EQ(r.x, 0);
    EXPECT_EQ(r.y, 30);
    EXPECT_EQ(r.width, 1024);
    EXPECT_EQ(r.height, 738);
}

TEST(ReadStrutsTest, partial_strut_is_preferred)
{
    FakeClient c;
    c.set_cardinals(dsp()._NET_WM_STRUT, {1, 2, 3, 4});
    c.set_cardinals(dsp()._NET_WM_STRUT_PARTIAL,
                    {10, 20, 30, 40, 0, 0, 0, 0, 0, 0, 0, 0});

    const Struts s = read_struts(c.window);

    EXPECT_EQ(s.left, 10);
    EXPECT_EQ(s.right, 20);
    EXPECT_EQ(s.top, 30);
    EXPECT_EQ(s.bottom, 40);
}

TEST(ReadStrutsTest, plain_strut_is_the_fallback)
{
    FakeClient c;
    c.set_cardinals(dsp()._NET_WM_STRUT, {1, 2, 3, 4});

    const Struts s = read_struts(c.window);

    EXPECT_EQ(s.left, 1);
    EXPECT_EQ(s.bottom, 4);
}

TEST(ReadStrutsTest, no_property_or_a_short_one_means_no_strut)
{
    FakeClient c;
    EXPECT_EQ(read_struts(c.window).top, 0);

    c.set_cardinals(dsp()._NET_WM_STRUT_PARTIAL, {5, 5, 5});
    const Struts s = read_struts(c.window);

    EXPECT_EQ(s.left, 0);
    EXPECT_EQ(s.top, 0);
}

TEST(ReadStrutsTest, a_negative_strut_is_zero)
{
    FakeClient c;
    c.set_cardinals(dsp()._NET_WM_STRUT, {-5, 0, 0, 0});

    EXPECT_EQ(read_struts(c.window).left, 0);
}

TEST(WorkAreaTest, inner_area_leaves_room_for_the_borders)
{
    const tiny::Rect r = tiny::wm::inner_area({0, 30, 1000, 700}, 2);
    EXPECT_EQ(r.x, 2);
    EXPECT_EQ(r.y, 32);
    EXPECT_EQ(r.width, 996);
    EXPECT_EQ(r.height, 696);
}

TEST(WorkAreaTest, inner_area_of_a_tiny_area_is_not_empty)
{
    const tiny::Rect r = tiny::wm::inner_area({0, 0, 2, 2}, 5);
    EXPECT_EQ(r.width, 1);
    EXPECT_EQ(r.height, 1);
}
