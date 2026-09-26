#include "lamp/sdk/lamp_sdk.h"

#include "lamp/core/model.h"
#include "lamp/core/protocol.h"
#include "lamp/core/units.h"
#include "lamp/logging/logger.h"
#include "lamp/platform/port_discovery.h"
#include "lamp/service/lamp_device.h"
#include "lamp/transport/serial_transport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// The SDK is a thin C facade over the LampDevice C++ service. Instances are
// not thread-safe; callers must serialize access per handle.
struct lamp_sdk {
    mutable std::mutex mtx;
    lamp::Logger logger;
    std::unique_ptr<lamp::LampDevice> device;
    std::string initial_port;
    int initial_baud = lamp::kDefaultBaudRate;
    bool restore_stable_mode = true;
    std::string last_error;
};

namespace {

std::mutex g_global_mutex;
std::string g_global_error;

void SetGlobalError(const std::string& message) {
    std::lock_guard<std::mutex> lock(g_global_mutex);
    g_global_error = message;
}

int ToResult(lamp::Result result) {
    switch (result) {
    case lamp::Result::Ok:
        return LAMP_SDK_OK;
    case lamp::Result::InvalidArgument:
        return LAMP_SDK_INVALID_ARGUMENT;
    case lamp::Result::NotConnected:
        return LAMP_SDK_NOT_CONNECTED;
    case lamp::Result::SerialError:
        return LAMP_SDK_SERIAL_ERROR;
    case lamp::Result::Error:
        return LAMP_SDK_ERROR;
    }
    return LAMP_SDK_ERROR;
}

// Writes "item;item;..." into the buffer. Returns the item count, or 0 when
// the buffer is too small.
int JoinPorts(char* buffer, size_t buffer_size,
              const std::vector<std::string>& ports) {
    size_t needed = 0;
    for (const auto& port : ports) {
        needed += port.size() + 1;
    }
    if (buffer == nullptr || needed >= buffer_size) {
        return 0;
    }
    size_t cursor = 0;
    for (const auto& port : ports) {
        std::memcpy(buffer + cursor, port.c_str(), port.size());
        cursor += port.size();
        buffer[cursor++] = ';';
    }
    buffer[cursor] = '\0';
    return static_cast<int>(ports.size());
}

}  // namespace

extern "C" {

LAMP_SDK_API const char *lamp_sdk_version(void) {
    return "1.0.0";
}

LAMP_SDK_API int lamp_sdk_percent_to_raw(int percent) {
    return lamp::PercentToRaw(percent);
}

LAMP_SDK_API int lamp_sdk_raw_to_percent(int raw_value) {
    return lamp::RawToPercent(raw_value);
}

LAMP_SDK_API int lamp_sdk_get_port_names(char *buffer, size_t buffer_size) {
    auto discovery = lamp::CreateWindowsPortDiscovery();
    return JoinPorts(buffer, buffer_size, discovery->GetAllPorts());
}

LAMP_SDK_API int lamp_sdk_get_usb_port_names(char *buffer, size_t buffer_size) {
    auto discovery = lamp::CreateWindowsPortDiscovery();
    return JoinPorts(buffer, buffer_size, discovery->GetPreferredPorts());
}

LAMP_SDK_API int lamp_sdk_find_preferred_port(char *buffer, size_t buffer_size) {
    auto discovery = lamp::CreateWindowsPortDiscovery();
    auto found = discovery->FindPreferredPort();
    if (!found) {
        SetGlobalError("未找到唯一串口，请指定端口。");
        if (buffer != nullptr && buffer_size > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }
    if (found->size() >= buffer_size) {
        return 0;
    }
    std::memcpy(buffer, found->c_str(), found->size() + 1);
    return 1;
}

LAMP_SDK_API int lamp_sdk_build_channel_command(
    int channel,
    int raw_value,
    char *buffer,
    size_t buffer_size
) {
    if (!lamp::IsValidChannel(channel) || !lamp::IsValidRaw(raw_value)) {
        return LAMP_SDK_INVALID_ARGUMENT;
    }
    std::string command =
        lamp::protocol::Build({lamp::protocol::CommandType::SetChannel,
                               channel, raw_value, "", 0});
    if (buffer == nullptr || command.size() >= buffer_size) {
        return LAMP_SDK_ERROR;
    }
    std::memcpy(buffer, command.c_str(), command.size() + 1);
    return LAMP_SDK_OK;
}

LAMP_SDK_API lamp_sdk_t *lamp_sdk_create(const char *port_name, int baud_rate) {
    auto *sdk = new lamp_sdk;
    sdk->logger.SetLevel(lamp::LogLevel::Error);
    sdk->initial_port = (port_name != nullptr && port_name[0] != '\0')
                            ? port_name
                            : std::string("auto");
    sdk->initial_baud = lamp::IsValidBaudRate(baud_rate) ? baud_rate
                                                         : lamp::kDefaultBaudRate;

    lamp::DeviceNotifier notifier;
    auto transport = lamp::CreateWindowsSerialTransport();
    auto discovery = lamp::CreateWindowsPortDiscovery();
    sdk->device = std::make_unique<lamp::LampDevice>(
        std::move(transport), std::move(discovery), sdk->logger,
        std::move(notifier), lamp::LampDevice::Options{false});
    return sdk;
}

LAMP_SDK_API void lamp_sdk_destroy(lamp_sdk_t *sdk) {
    delete sdk;
}

LAMP_SDK_API int lamp_sdk_connect(lamp_sdk_t *sdk) {
    if (sdk == nullptr) {
        SetGlobalError("无效的 SDK 句柄。");
        return LAMP_SDK_ERROR;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result =
        sdk->device->Connect(sdk->initial_port, sdk->initial_baud);
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API int lamp_sdk_connect_to(lamp_sdk_t *sdk, const char *port_name) {
    if (sdk == nullptr || port_name == nullptr || port_name[0] == '\0') {
        SetGlobalError("无效的串口名。");
        return LAMP_SDK_ERROR;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result =
        sdk->device->Connect(port_name, sdk->initial_baud);
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API void lamp_sdk_disconnect(lamp_sdk_t *sdk) {
    if (sdk == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    sdk->device->Disconnect();
}

LAMP_SDK_API int lamp_sdk_is_connected(const lamp_sdk_t *sdk) {
    if (sdk == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    return sdk->device->IsConnected() ? 1 : 0;
}

LAMP_SDK_API const char *lamp_sdk_get_last_error(const lamp_sdk_t *sdk) {
    if (sdk != nullptr) {
        return sdk->last_error.c_str();
    }
    std::lock_guard<std::mutex> lock(g_global_mutex);
    return g_global_error.c_str();
}

LAMP_SDK_API int lamp_sdk_set_restore_stable_mode(lamp_sdk_t *sdk, int enabled) {
    if (sdk == nullptr) {
        return LAMP_SDK_ERROR;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    sdk->restore_stable_mode = enabled != 0;
    sdk->device->SetRestoreStableMode(sdk->restore_stable_mode);
    return LAMP_SDK_OK;
}

LAMP_SDK_API int lamp_sdk_get_restore_stable_mode(const lamp_sdk_t *sdk) {
    if (sdk == nullptr) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    return sdk->restore_stable_mode ? 1 : 0;
}

LAMP_SDK_API int lamp_sdk_set_brightness_percent(
    lamp_sdk_t *sdk,
    int channel,
    int percent
) {
    if (sdk == nullptr) {
        SetGlobalError("无效的 SDK 句柄。");
        return LAMP_SDK_ERROR;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result = sdk->device->SetChannel(channel, percent);
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API int lamp_sdk_set_brightness_percent_all(
    lamp_sdk_t *sdk,
    const int *percents
) {
    if (sdk == nullptr || percents == nullptr) {
        SetGlobalError("无效的参数。");
        return LAMP_SDK_ERROR;
    }
    std::array<int, lamp::kChannelCount> values{};
    for (int index = 0; index < lamp::kChannelCount; index++) {
        values[index] = percents[index];
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result = sdk->device->SetChannels(values);
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API int lamp_sdk_turn_on(lamp_sdk_t *sdk, int channel, int percent) {
    return lamp_sdk_set_brightness_percent(sdk, channel, percent);
}

LAMP_SDK_API int lamp_sdk_turn_on_all(lamp_sdk_t *sdk, int percent) {
    if (sdk == nullptr) {
        SetGlobalError("无效的 SDK 句柄。");
        return LAMP_SDK_ERROR;
    }
    std::array<int, lamp::kChannelCount> values{};
    values.fill(percent);
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result = sdk->device->SetChannels(values);
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API int lamp_sdk_turn_off(lamp_sdk_t *sdk, int channel) {
    return lamp_sdk_set_brightness_percent(sdk, channel, 0);
}

LAMP_SDK_API int lamp_sdk_turn_off_all(lamp_sdk_t *sdk) {
    if (sdk == nullptr) {
        SetGlobalError("无效的 SDK 句柄。");
        return LAMP_SDK_ERROR;
    }
    std::array<int, lamp::kChannelCount> values{};
    values.fill(0);
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result = sdk->device->SetChannels(values);
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API int lamp_sdk_apply_settings(lamp_sdk_t *sdk) {
    if (sdk == nullptr) {
        SetGlobalError("无效的 SDK 句柄。");
        return LAMP_SDK_ERROR;
    }
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result = sdk->device->Apply();
    sdk->last_error = sdk->device->LastError();
    return ToResult(result);
}

LAMP_SDK_API int lamp_sdk_query_status(
    lamp_sdk_t *sdk,
    lamp_sdk_status_t *status
) {
    if (sdk == nullptr || status == nullptr) {
        SetGlobalError("无效的参数。");
        return LAMP_SDK_ERROR;
    }
    lamp::DeviceStatus device_status;
    std::lock_guard<std::mutex> lock(sdk->mtx);
    lamp::Result result = sdk->device->QueryStatus(device_status);
    sdk->last_error = sdk->device->LastError();
    if (result != lamp::Result::Ok) {
        return ToResult(result);
    }

    std::memset(status, 0, sizeof(*status));
    std::string port = sdk->device->PortName();
    std::strncpy(status->port_name, port.c_str(), sizeof(status->port_name) - 1);
    status->baud_rate = sdk->device->BaudRate();
    status->version = device_status.version;
    status->mode = device_status.mode;
    status->pwm = device_status.pwm;
    status->control = device_status.control;
    status->temperature = device_status.temperature;
    status->voltage_raw = device_status.voltage_raw;
    status->current_raw = device_status.current_raw;
    status->error_flags = device_status.error_flags;
    return LAMP_SDK_OK;
}

}  // extern "C"
