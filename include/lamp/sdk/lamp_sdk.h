#ifndef LAMP_SDK_H
#define LAMP_SDK_H

#include <stddef.h>

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

#define LAMP_SDK_DEFAULT_BAUD_RATE 19200
#define LAMP_SDK_MAX_RAW_VALUE 255
#define LAMP_SDK_PORT_NAME_SIZE 32
#define LAMP_SDK_CHANNEL_COUNT 4

typedef struct lamp_sdk lamp_sdk_t;

typedef struct lamp_sdk_status {
    char port_name[LAMP_SDK_PORT_NAME_SIZE];
    int baud_rate;
    int version;
    int mode;
    int pwm;
    int control;
    int temperature;
    int voltage_raw;
    int current_raw;
    int error_flags;
} lamp_sdk_status_t;

typedef enum lamp_sdk_result {
    LAMP_SDK_OK = 0,
    LAMP_SDK_ERROR = -1,
    LAMP_SDK_INVALID_ARGUMENT = -2,
    LAMP_SDK_NOT_CONNECTED = -3,
    LAMP_SDK_SERIAL_ERROR = -4
} lamp_sdk_result_t;

LAMP_SDK_API const char *lamp_sdk_version(void);

LAMP_SDK_API int lamp_sdk_percent_to_raw(int percent);
LAMP_SDK_API int lamp_sdk_raw_to_percent(int raw_value);

LAMP_SDK_API int lamp_sdk_get_port_names(char *buffer, size_t buffer_size);
LAMP_SDK_API int lamp_sdk_get_usb_port_names(char *buffer, size_t buffer_size);
LAMP_SDK_API int lamp_sdk_find_preferred_port(char *buffer, size_t buffer_size);
LAMP_SDK_API int lamp_sdk_build_channel_command(
    int channel,
    int raw_value,
    char *buffer,
    size_t buffer_size
);

LAMP_SDK_API lamp_sdk_t *lamp_sdk_create(const char *port_name, int baud_rate);
LAMP_SDK_API void lamp_sdk_destroy(lamp_sdk_t *sdk);

LAMP_SDK_API int lamp_sdk_connect(lamp_sdk_t *sdk);
LAMP_SDK_API int lamp_sdk_connect_to(lamp_sdk_t *sdk, const char *port_name);
LAMP_SDK_API void lamp_sdk_disconnect(lamp_sdk_t *sdk);
LAMP_SDK_API int lamp_sdk_is_connected(const lamp_sdk_t *sdk);
LAMP_SDK_API const char *lamp_sdk_get_last_error(const lamp_sdk_t *sdk);

LAMP_SDK_API int lamp_sdk_set_restore_stable_mode(lamp_sdk_t *sdk, int enabled);
LAMP_SDK_API int lamp_sdk_get_restore_stable_mode(const lamp_sdk_t *sdk);

// Channels are numbered 1-4. Brightness is a percentage in 0-100.
LAMP_SDK_API int lamp_sdk_set_brightness_percent(
    lamp_sdk_t *sdk,
    int channel,
    int percent
);

LAMP_SDK_API int lamp_sdk_set_brightness_percent_all(
    lamp_sdk_t *sdk,
    const int *percents
);

LAMP_SDK_API int lamp_sdk_turn_on(lamp_sdk_t *sdk, int channel, int percent);
LAMP_SDK_API int lamp_sdk_turn_on_all(lamp_sdk_t *sdk, int percent);
LAMP_SDK_API int lamp_sdk_turn_off(lamp_sdk_t *sdk, int channel);
LAMP_SDK_API int lamp_sdk_turn_off_all(lamp_sdk_t *sdk);
LAMP_SDK_API int lamp_sdk_apply_settings(lamp_sdk_t *sdk);

LAMP_SDK_API int lamp_sdk_query_status(
    lamp_sdk_t *sdk,
    lamp_sdk_status_t *status
);

#ifdef __cplusplus
}
#endif

#endif
