#include <gtest/gtest.h>

#include <vector>

#include "wm/recent_list.h"

using tiny::wm::RecentList;

namespace {

using Items = std::vector<int>;

} // namespace

TEST(RecentListTest, is_empty_at_first)
{
    const RecentList<int> list;

    EXPECT_TRUE(list.items().empty());
}

TEST(RecentListTest, the_item_used_last_is_first)
{
    RecentList<int> list;

    list.touch(1);
    list.touch(2);
    list.touch(3);

    EXPECT_EQ(list.items(), (Items{3, 2, 1}));
}

TEST(RecentListTest, an_item_used_again_moves_to_the_front_once)
{
    RecentList<int> list;
    list.touch(1);
    list.touch(2);
    list.touch(3);

    list.touch(1);

    EXPECT_EQ(list.items(), (Items{1, 3, 2}));
}

TEST(RecentListTest, a_new_item_that_was_not_used_goes_to_the_end)
{
    RecentList<int> list;
    list.touch(1);

    list.add_last(2);
    list.add_last(3);

    EXPECT_EQ(list.items(), (Items{1, 2, 3}));
}

TEST(RecentListTest, adding_a_known_item_keeps_its_place)
{
    RecentList<int> list;
    list.touch(1);
    list.touch(2);

    list.add_last(2);

    EXPECT_EQ(list.items(), (Items{2, 1}));
}

TEST(RecentListTest, removes_an_item)
{
    RecentList<int> list;
    list.touch(1);
    list.touch(2);
    list.touch(3);

    list.remove(2);
    list.remove(9); // unknown: nothing happens

    EXPECT_EQ(list.items(), (Items{3, 1}));
    EXPECT_TRUE(list.contains(3));
    EXPECT_FALSE(list.contains(2));
}
