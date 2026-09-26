#include "test_harness.h"

#include "lamp/config/app_config.h"

#include <cstdio>

using namespace lamp;

namespace {
const char* kConfigPath = "test_config.ini";
}

TEST(config_round_trip) {
    AppConfigData config;
    config.port_name = "COM7";
    config.baud_rate = 115200;
    config.mode = 1;
    config.pwm = 0;
    config.restore_stable_mode = false;
    config.channels = {10, 20, 30, 40};

    SaveAppConfig(kConfigPath, config);
    AppConfigData loaded = LoadAppConfig(kConfigPath);
    EXPECT_EQ(loaded.port_name, config.port_name);
    EXPECT_EQ(loaded.baud_rate, config.baud_rate);
    EXPECT_EQ(loaded.mode, config.mode);
    EXPECT_EQ(loaded.pwm, config.pwm);
    EXPECT_EQ(loaded.restore_stable_mode, config.restore_stable_mode);
    for (int index = 0; index < kChannelCount; index++) {
        EXPECT_EQ(loaded.channels[index], config.channels[index]);
    }
    std::remove(kConfigPath);
}

TEST(config_missing_file_returns_defaults) {
    std::remove(kConfigPath);
    AppConfigData config = LoadAppConfig(kConfigPath);
    EXPECT_EQ(config.baud_rate, kDefaultBaudRate);
    EXPECT_TRUE(config.restore_stable_mode);
    EXPECT_EQ(config.mode, 0);
    EXPECT_EQ(config.pwm, 1);
    EXPECT_TRUE(config.port_name.empty());
    EXPECT_EQ(config.channels[2], 0);
}

TEST(config_malformed_lines_ignored) {
    FILE* file = std::fopen(kConfigPath, "w");
    if (file) {
        std::fputs("baud_rate=9600\n;; comment\nnot_a_key=123\nchannel2=55\n"
                   "bad line without equals\n",
                   file);
        std::fclose(file);
    }
    AppConfigData config = LoadAppConfig(kConfigPath);
    EXPECT_EQ(config.baud_rate, 9600);
    EXPECT_EQ(config.channels[1], 55);
    EXPECT_EQ(config.channels[0], 0);
    EXPECT_EQ(config.mode, 0);
    std::remove(kConfigPath);
}

TEST(config_whitespace_and_invalid_numbers) {
    FILE* file = std::fopen(kConfigPath, "w");
    if (file) {
        std::fputs(" baud_rate = 38400 \nrestore_stable_mode=0\n"
                   "pwm=abc\nchannel3=90\n",
                   file);
        std::fclose(file);
    }
    AppConfigData config = LoadAppConfig(kConfigPath);
    EXPECT_EQ(config.baud_rate, 38400);
    EXPECT_FALSE(config.restore_stable_mode);
    EXPECT_EQ(config.pwm, 1);
    EXPECT_EQ(config.channels[2], 90);
    std::remove(kConfigPath);
}
