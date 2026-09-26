#pragma once

#include "lamp/core/model.h"

#include <array>
#include <functional>
#include <memory>
#include <string>

namespace lamp {

class SerialTransport;
class PortDiscovery;
class Logger;

// Observer slots invoked from the device worker thread. Consumers that touch
// UI state must marshal to their own thread (for example via PostMessage).
struct DeviceNotifier {
    std::function<void(ConnectionState)> on_state;
    std::function<void(bool)> on_connected;
    std::function<void(int channel, const ChannelState&)> on_channel;
    std::function<void(const DeviceStatus&)> on_status;
    std::function<void(const std::string&)> on_log;
};

// Facade over transport, protocol and connection lifecycle. Owns a worker
// thread; all serial I/O and state mutation happen there. Async methods return
// immediately and deliver results through the notifier; sync methods block
// until the operation completes.
class LampDevice {
public:
    struct Options {
        bool auto_reconnect = true;
    };

    LampDevice(std::unique_ptr<SerialTransport> transport,
               std::unique_ptr<PortDiscovery> discovery, Logger& logger,
               DeviceNotifier notifier);
    LampDevice(std::unique_ptr<SerialTransport> transport,
               std::unique_ptr<PortDiscovery> discovery, Logger& logger,
               DeviceNotifier notifier, Options options);
    ~LampDevice();

    LampDevice(const LampDevice&) = delete;
    LampDevice& operator=(const LampDevice&) = delete;

    void ConnectAsync(const std::string& port_name, int baud_rate);
    void DisconnectAsync();
    void SetChannelAsync(int channel, int percent);
    void SetChannelRawAsync(int channel, int raw);
    void SetChannelsAsync(const std::array<int, kChannelCount>& percents);
    void ApplyAsync();
    void QueryStatusAsync();

    Result Connect(const std::string& port_name, int baud_rate);
    Result Disconnect();
    Result SetChannel(int channel, int percent);
    Result SetChannelRaw(int channel, int raw);
    Result SetChannels(const std::array<int, kChannelCount>& percents);
    Result Apply();
    Result QueryStatus(DeviceStatus& status);
    Result SendRaw(const std::string& command, int read_ms,
                   std::string& response);

    void SetMode(int mode);
    void SetPwm(int pwm);
    void SetRestoreStableMode(bool enabled);
    void SetAutoReconnect(bool enabled);

    ConnectionState State() const;
    bool IsConnected() const;
    std::string LastError() const;
    std::string PortName() const;
    int BaudRate() const;
    int Mode() const;
    int Pwm() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace lamp
