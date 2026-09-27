#pragma once

#include <array>
#include <string>

namespace lamp {

constexpr int kMinChannel = 1;
constexpr int kMaxChannel = 4;
constexpr int kChannelCount = 4;
constexpr int kMaxRawValue = 255;
constexpr int kDefaultBaudRate = 19200;
constexpr size_t kErrorSize = 256;

enum class Result {
    Ok = 0,
    Error = -1,
    InvalidArgument = -2,
    NotConnected = -3,
    SerialError = -4
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

struct ChannelState {
    bool on = false;
    int percent = 0;
    int raw = 0;
};

struct DeviceStatus {
    int version = -1;
    int mode = -1;
    int pwm = -1;
    int control = -1;
    int temperature = -1;
    int voltage_raw = -1;
    int current_raw = -1;
    int error_flags = -1;
};

std::string ToString(ConnectionState state);

}  // namespace lamp
