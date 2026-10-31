#include <memory>

#include <gtest/gtest.h>

#include "button.h"

TEST(MemoryTest, create_and_destroy_buttons)
{
    for (int i = 0; i < 1000; ++i) {
        auto item = std::make_shared<tiny::LabelButton>(100, 28, "Menu Item");
    }
}
