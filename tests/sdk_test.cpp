// SDK C ABI 自动化测试：只覆盖不依赖真实设备的部分，
// 串口连接、状态查询等需真机验证的路径不在本文件范围。
#include "test_harness.h"

#include "lamp/sdk/lamp_sdk.h"

#include <cstring>

TEST(sdk_version) {
    EXPECT_EQ(std::strcmp(lamp_sdk_version(), "1.0.0"), 0);
}

TEST(sdk_percent_raw_conversion) {
    EXPECT_EQ(lamp_sdk_percent_to_raw(0), 0);
    EXPECT_EQ(lamp_sdk_percent_to_raw(50), 128);
    EXPECT_EQ(lamp_sdk_percent_to_raw(100), 255);
    EXPECT_EQ(lamp_sdk_raw_to_percent(0), 0);
    EXPECT_EQ(lamp_sdk_raw_to_percent(128), 50);
    EXPECT_EQ(lamp_sdk_raw_to_percent(255), 100);
}

TEST(sdk_build_channel_command) {
    char buffer[LAMP_SDK_PORT_NAME_SIZE] = {};
    EXPECT_EQ(lamp_sdk_build_channel_command(1, 153, buffer, sizeof(buffer)),
              LAMP_SDK_OK);
    EXPECT_EQ(std::strcmp(buffer, "SPA153#"), 0);

    // 通道与原始值越界返回参数错误。
    EXPECT_EQ(lamp_sdk_build_channel_command(0, 0, buffer, sizeof(buffer)),
              LAMP_SDK_INVALID_ARGUMENT);
    EXPECT_EQ(lamp_sdk_build_channel_command(5, 0, buffer, sizeof(buffer)),
              LAMP_SDK_INVALID_ARGUMENT);
    EXPECT_EQ(lamp_sdk_build_channel_command(1, 256, buffer, sizeof(buffer)),
              LAMP_SDK_INVALID_ARGUMENT);

    // 缓冲区过小返回一般错误。
    EXPECT_EQ(lamp_sdk_build_channel_command(1, 0, buffer, 2), LAMP_SDK_ERROR);
}

TEST(sdk_handle_lifecycle_without_device) {
    lamp_sdk_t* sdk = lamp_sdk_create("COM9", 19200);
    EXPECT_TRUE(sdk != nullptr);

    EXPECT_EQ(lamp_sdk_is_connected(sdk), 0);
    EXPECT_EQ(lamp_sdk_get_restore_stable_mode(sdk), 1);  // 默认启用
    EXPECT_EQ(lamp_sdk_set_restore_stable_mode(sdk, 0), LAMP_SDK_OK);
    EXPECT_EQ(lamp_sdk_get_restore_stable_mode(sdk), 0);

    // 未连接时所有控制/查询接口返回 NOT_CONNECTED。
    EXPECT_EQ(lamp_sdk_set_brightness_percent(sdk, 1, 50),
              LAMP_SDK_NOT_CONNECTED);
    EXPECT_EQ(lamp_sdk_turn_off(sdk, 2), LAMP_SDK_NOT_CONNECTED);
    EXPECT_EQ(lamp_sdk_apply_settings(sdk), LAMP_SDK_NOT_CONNECTED);
    lamp_sdk_status_t status;
    EXPECT_EQ(lamp_sdk_query_status(sdk, &status), LAMP_SDK_NOT_CONNECTED);
    EXPECT_TRUE(lamp_sdk_get_last_error(sdk) != nullptr);

    lamp_sdk_destroy(sdk);
}

TEST(sdk_null_handle_safety) {
    EXPECT_EQ(lamp_sdk_is_connected(nullptr), 0);
    EXPECT_EQ(lamp_sdk_get_restore_stable_mode(nullptr), 0);
    EXPECT_EQ(lamp_sdk_set_brightness_percent(nullptr, 1, 50), LAMP_SDK_ERROR);
    EXPECT_EQ(lamp_sdk_set_brightness_percent_all(nullptr, nullptr),
              LAMP_SDK_ERROR);
    EXPECT_EQ(lamp_sdk_turn_off_all(nullptr), LAMP_SDK_ERROR);
    EXPECT_EQ(lamp_sdk_apply_settings(nullptr), LAMP_SDK_ERROR);
    EXPECT_TRUE(lamp_sdk_get_last_error(nullptr) != nullptr);
    lamp_sdk_disconnect(nullptr);  // 文档约定为安全返回。
}

TEST(sdk_port_enumeration) {
    char buffer[256];
    int count = lamp_sdk_get_port_names(buffer, sizeof(buffer));
    EXPECT_LE(0, count);
    EXPECT_EQ(lamp_sdk_get_port_names(nullptr, 0), 0);
    EXPECT_EQ(lamp_sdk_get_usb_port_names(buffer, 0), 0);
}
