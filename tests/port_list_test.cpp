#include "test_harness.h"

#include "lamp/platform/port_discovery.h"

using namespace lamp;

TEST(pick_preferred_single_usb_port) {
    EXPECT_EQ(PickPreferredPort({"COM1", "COM9"}, {"COM9"}).value(),
              std::string("COM9"));
}

TEST(pick_preferred_single_plain_port) {
    EXPECT_EQ(PickPreferredPort({"COM3"}, {}).value(), std::string("COM3"));
}

TEST(pick_preferred_empty) {
    EXPECT_FALSE(PickPreferredPort({}, {}).has_value());
}

TEST(pick_preferred_multiple_ambiguous) {
    EXPECT_FALSE(PickPreferredPort({"COM1", "COM2"}, {}).has_value());
    EXPECT_FALSE(PickPreferredPort({"COM1", "COM2"}, {"COM1", "COM2"}).has_value());
}

TEST(pick_preferred_prefers_usb_over_plain) {
    EXPECT_EQ(PickPreferredPort({"COM3", "COM7"}, {"COM7"}).value(),
              std::string("COM7"));
}
