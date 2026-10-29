#include <gtest/gtest.h>

#include "display.h"

// Every test needs an open X display (run under xvfb-run if there is none).
class DisplayEnvironment : public ::testing::Environment {
  public:
    void SetUp() override { tiny::Display::init(); }
};

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new DisplayEnvironment);
    return RUN_ALL_TESTS();
}
