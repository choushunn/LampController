#ifndef LAMP_SDK_H
#define LAMP_SDK_H

#include <stddef.h>

/*
 * LampController C SDK
 *
 * 通过串口控制智能灯光控制器的原生 C 动态库接口。
 *
 * 典型使用流程：
 *   1. lamp_sdk_get_port_names / lamp_sdk_find_preferred_port 选择端口；
 *   2. lamp_sdk_create 创建句柄（端口名传 NULL 或空串时连接阶段自动选择）；
 *   3. lamp_sdk_connect 连接设备；
 *   4. lamp_sdk_set_brightness_percent / lamp_sdk_turn_on 等下发控制；
 *   5. lamp_sdk_destroy 释放句柄。
 *
 * 返回约定：成功返回非负值（通常为 LAMP_SDK_OK=0），失败返回 lamp_sdk_result_t
 * 中的负错误码，详细错误文本可通过 lamp_sdk_get_last_error 获取。
 * 线程安全：除 lamp_sdk_destroy 外所有函数均可跨线程调用，内部已加锁。
 */

#if defined(_WIN32) && defined(LAMP_SDK_EXPORTS)
#define LAMP_SDK_API __declspec(dllexport)
#elif defined(_WIN32)
#define LAMP_SDK_API __declspec(dllimport)
#else
#define LAMP_SDK_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define LAMP_SDK_DEFAULT_BAUD_RATE 19200  // 默认波特率
#define LAMP_SDK_MAX_RAW_VALUE 255        // 亮度原始值上限
#define LAMP_SDK_PORT_NAME_SIZE 32        // 端口名字符串最大长度（含结尾 NUL）
#define LAMP_SDK_CHANNEL_COUNT 4          // 支持的通道总数

// SDK 句柄，由 lamp_sdk_create 创建、lamp_sdk_destroy 释放。
typedef struct lamp_sdk lamp_sdk_t;

// 设备状态快照，由 lamp_sdk_query_status 填充。
typedef struct lamp_sdk_status {
    char port_name[LAMP_SDK_PORT_NAME_SIZE];  // 当前连接的端口名，未连接时为空串
    int baud_rate;  // 当前波特率
    int version;    // 固件版本（解析自 S_VER 响应，未知时为 0）
    int mode;       // 模式：0=稳定数字调光，1=频闪
    int pwm;        // PWM 频率：0=47.5K，1=95K
    int control;    // 输出控制：1=允许通道输出，0=禁止
    int temperature;  // 温度原始值
    int voltage_raw;  // 电压原始值
    int current_raw;  // 电流原始值
    int error_flags;  // 保护错误码（解析自 S_ERRS 响应）
} lamp_sdk_status_t;

// 返回值错误码。
typedef enum lamp_sdk_result {
    LAMP_SDK_OK = 0,            // 成功
    LAMP_SDK_ERROR = -1,        // 一般性失败，详情见 lamp_sdk_get_last_error
    LAMP_SDK_INVALID_ARGUMENT = -2,  // 参数非法（如通道/亮度越界、空指针）
    LAMP_SDK_NOT_CONNECTED = -3,     // 未连接设备
    LAMP_SDK_SERIAL_ERROR = -4       // 串口通信错误
} lamp_sdk_result_t;

// 返回 SDK 版本字符串（如 "1.0.0"），静态常量，无需释放。
LAMP_SDK_API const char *lamp_sdk_version(void);

// 百分比亮度（0-100）与设备原始值（0-255）互相换算。
LAMP_SDK_API int lamp_sdk_percent_to_raw(int percent);
LAMP_SDK_API int lamp_sdk_raw_to_percent(int raw_value);

// 将系统全部串口名写入 buffer。返回写入名称的数量，0 表示未发现任何端口。
LAMP_SDK_API int lamp_sdk_get_port_names(char *buffer, size_t buffer_size);
// 仅枚举 USB 转串口的端口名，其余语义同 lamp_sdk_get_port_names。
LAMP_SDK_API int lamp_sdk_get_usb_port_names(char *buffer, size_t buffer_size);
// 当且仅当唯一串口时返回 1 并把端口名写入 buffer；0 表示串口不唯一或不存在，
// 此时错误文本见 lamp_sdk_get_last_error。
LAMP_SDK_API int lamp_sdk_find_preferred_port(char *buffer, size_t buffer_size);
// 将通道亮度原始指令（如 SPA153#）构建到 buffer。
// channel 取值 1-4，raw_value 取值 0-255；成功返回 LAMP_SDK_OK。
LAMP_SDK_API int lamp_sdk_build_channel_command(
    int channel,
    int raw_value,
    char *buffer,
    size_t buffer_size
);

// 创建 SDK 句柄。port_name 传 NULL 或空串表示连接时自动选择端口；
// baud_rate 非法时回退到 LAMP_SDK_DEFAULT_BAUD_RATE。失败返回 NULL。
// 创建后默认关闭自动重连，与 lamp_gui 的自动重连行为不同。
LAMP_SDK_API lamp_sdk_t *lamp_sdk_create(const char *port_name, int baud_rate);
// 销毁句柄并释放全部资源。销毁后该句柄不得再使用，也不应与其它调用并发。
LAMP_SDK_API void lamp_sdk_destroy(lamp_sdk_t *sdk);

// 连接设备（阻断式）。connect 使用 create 时指定的端口与波特率，此后可用
// connect_to 改连指定端口。成功返回 LAMP_SDK_OK。
LAMP_SDK_API int lamp_sdk_connect(lamp_sdk_t *sdk);
LAMP_SDK_API int lamp_sdk_connect_to(lamp_sdk_t *sdk, const char *port_name);
// 断开连接。sdk 为 NULL 时安全返回。
LAMP_SDK_API void lamp_sdk_disconnect(lamp_sdk_t *sdk);
// 返回 1 表示已连接，0 表示未连接或 sdk 为 NULL。
LAMP_SDK_API int lamp_sdk_is_connected(const lamp_sdk_t *sdk);
// 返回最近一次失败的错误文本（含 sdk 为 NULL 时的全局错误）。返回值指向
// SDK 内部缓存，在调用下一个 SDK 函数前有效，不可手动释放。
LAMP_SDK_API const char *lamp_sdk_get_last_error(const lamp_sdk_t *sdk);

// 启用/查询"连接后自动恢复稳定数字调光"选项。enabled 非 0 表示启用。
LAMP_SDK_API int lamp_sdk_set_restore_stable_mode(lamp_sdk_t *sdk, int enabled);
LAMP_SDK_API int lamp_sdk_get_restore_stable_mode(const lamp_sdk_t *sdk);

// 设置指定通道（1-4）亮度百分比（0-100），成功返回 LAMP_SDK_OK。
// 未连接时返回 LAMP_SDK_NOT_CONNECTED。
LAMP_SDK_API int lamp_sdk_set_brightness_percent(
    lamp_sdk_t *sdk,
    int channel,
    int percent
);

// 一次性设置全部通道亮度。percents 必须指向长度为
// LAMP_SDK_CHANNEL_COUNT（4）的 int 数组，按通道 1-4 顺序排列。
LAMP_SDK_API int lamp_sdk_set_brightness_percent_all(
    lamp_sdk_t *sdk,
    const int *percents
);

// 打开/关闭指定通道：turn_on 将通道设置为给定百分比亮度并开启输出，
// turn_off 关闭该通道输出。channel 取值 1-4。
LAMP_SDK_API int lamp_sdk_turn_on(lamp_sdk_t *sdk, int channel, int percent);
LAMP_SDK_API int lamp_sdk_turn_on_all(lamp_sdk_t *sdk, int percent);
LAMP_SDK_API int lamp_sdk_turn_off(lamp_sdk_t *sdk, int channel);
LAMP_SDK_API int lamp_sdk_turn_off_all(lamp_sdk_t *sdk);
// 下发 S_ALL 指令，使缓存的参数（模式/频率/亮度等）立即生效。
LAMP_SDK_API int lamp_sdk_apply_settings(lamp_sdk_t *sdk);

// 查询设备状态并填充 status 结构。成功返回 LAMP_SDK_OK；
// 未连接返回 LAMP_SDK_NOT_CONNECTED。status 不允许为 NULL。
LAMP_SDK_API int lamp_sdk_query_status(
    lamp_sdk_t *sdk,
    lamp_sdk_status_t *status
);

#ifdef __cplusplus
}
#endif

#endif