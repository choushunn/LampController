#include "test_harness.h"

#include "lamp/core/units.h"

using namespace lamp;

TEST(percent_to_raw_boundaries) {
    EXPECT_EQ(PercentToRaw(0), 0);
    EXPECT_EQ(PercentToRaw(50), 128);
    EXPECT_EQ(PercentToRaw(100), 255);
}

TEST(percent_to_raw_invalid) {
    EXPECT_EQ(PercentToRaw(-1), -1);
    EXPECT_EQ(PercentToRaw(101), -1);
}

TEST(raw_to_percent_boundaries) {
    EXPECT_EQ(RawToPercent(0), 0);
    EXPECT_EQ(RawToPercent(128), 50);
    EXPECT_EQ(RawToPercent(255), 100);
}

TEST(raw_to_percent_invalid) {
    EXPECT_EQ(RawToPercent(-1), -1);
    EXPECT_EQ(RawToPercent(256), -1);
}

TEST(percent_raw_round_trip) {
    for (int percent = 0; percent <= 100; percent += 5) {
        int raw = PercentToRaw(percent);
        EXPECT_TRUE(raw >= 0);
        EXPECT_TRUE(RawToPercent(raw) >= 0);
    }
}

TEST(channel_validation) {
    EXPECT_TRUE(IsValidChannel(1));
    EXPECT_TRUE(IsValidChannel(2));
    EXPECT_TRUE(IsValidChannel(3));
    EXPECT_TRUE(IsValidChannel(4));
    EXPECT_FALSE(IsValidChannel(0));
    EXPECT_FALSE(IsValidChannel(5));
}

TEST(percent_raw_validation) {
    EXPECT_TRUE(IsValidPercent(0));
    EXPECT_TRUE(IsValidPercent(100));
    EXPECT_FALSE(IsValidPercent(-1));
    EXPECT_FALSE(IsValidPercent(101));
    EXPECT_TRUE(IsValidRaw(0));
    EXPECT_TRUE(IsValidRaw(255));
    EXPECT_FALSE(IsValidRaw(-1));
    EXPECT_FALSE(IsValidRaw(256));
    EXPECT_TRUE(IsValidBaudRate(19200));
    EXPECT_FALSE(IsValidBaudRate(0));
    EXPECT_FALSE(IsValidBaudRate(-9600));
}
