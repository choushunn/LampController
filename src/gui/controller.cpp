#include "controller.h"

#include "lamp/core/units.h"
#include "lamp/platform/port_discovery.h"
#include "lamp/transport/serial_transport.h"

#include <cstdio>
#include <string>
#include <vector>

namespace lamp {
namespace gui {

Controller::Controller(UiModel& model, HWND hwnd, const AppConfigData& config)
    : model_(model),
      hwnd_(hwnd),
      config_(config),
      logger_(LogLevel::Info),
      discovery_(CreateWindowsPortDiscovery()),
      device_(std::make_unique<LampDevice>(
          CreateWindowsSerialTransport(), CreateWindowsPortDiscovery(),
          logger_,
          DeviceNotifier{
              [this](ConnectionState state) { PostState(state); },
              [this](bool connected) { PostConnected(connected); },
              [this](int channel, const ChannelState& state) {
                  PushChannel(channel, state);
              },
              [this](const DeviceStatus& status) { PushStatus(status); },
              [this](const std::string& text) { PushLog(text); },
          })) {
    logger_.SetFileSink(true, "logs");
    device_->SetMode(config_.mode);
    device_->SetPwm(config_.pwm);
    device_->SetRestoreStableMode(config_.restore_stable_mode);
    device_->SetAutoReconnect(true);
}

Controller::~Controller() {
    logger_.SetFileSink(false, "logs");
}

void Controller::PostState(ConnectionState state) {
    PostMessageW(hwnd_, kMsgState, static_cast<WPARAM>(state), 0);
}

void Controller::PostConnected(bool connected) {
    PostMessageW(hwnd_, kMsgConnected, connected ? 1 : 0, 0);
}

void Controller::PushChannel(int channel, const ChannelState& state) {
    {
        std::lock_guard<std::mutex> lock(pending_mtx_);
        pending_channels_.push_back(
            PendingChannel{channel, state.on, state.percent, state.raw});
    }
    PostMessageW(hwnd_, kMsgChannel, 0, 0);
}

void Controller::PushStatus(const DeviceStatus& status) {
    {
        std::lock_guard<std::mutex> lock(pending_mtx_);
        pending_statuses_.push_back(status);
    }
    PostMessageW(hwnd_, kMsgStatus, 0, 0);
}

void Controller::PushLog(const std::string& text) {
    {
        std::lock_guard<std::mutex> lock(pending_mtx_);
        pending_logs_.push_back(text);
    }
    PostMessageW(hwnd_, kMsgLog, 0, 0);
}

LRESULT Controller::HandleMessage(UINT message, WPARAM w_param,
                                  LPARAM l_param) {
    (void)l_param;
    switch (message) {
    case kMsgState:
        model_.connection_state =
            static_cast<ConnectionState>(static_cast<int>(w_param));
        switch (model_.connection_state) {
        case ConnectionState::Disconnected:
            model_.status_message = "设备状态：等待连接";
            break;
        case ConnectionState::Connecting:
            model_.status_message = "设备状态：正在连接...";
            break;
        case ConnectionState::Connected:
            model_.status_message =
                "设备状态：已连接 " + device_->PortName();
            break;
        case ConnectionState::Reconnecting:
            model_.status_message = "设备状态：正在重连";
            break;
        case ConnectionState::Failed:
            model_.status_message = "设备状态：连接失败";
            break;
        }
        break;

    case kMsgConnected: {
        bool connected = w_param != 0;
        model_.connected = connected;
        if (connected) {
            // 连接成功后把实际使用的端口写回配置并持久化。
            config_.port_name = device_->PortName();
            SaveConfig();
            model_.port_index = -1;
            for (size_t index = 0; index < model_.port_names.size(); ++index) {
                if (model_.port_names[index] == config_.port_name) {
                    model_.port_index = static_cast<int>(index);
                    break;
                }
            }
            if (model_.connection_state != ConnectionState::Connected) {
                model_.status_message = "设备状态：串口已打开，正在确认设备";
            }
        } else if (model_.connection_state == ConnectionState::Disconnected ||
                   model_.connection_state == ConnectionState::Failed) {
            model_.status_message = "设备状态：等待连接";
        }
        break;
    }

    case kMsgChannel: {
        PendingChannel pending;
        {
            std::lock_guard<std::mutex> lock(pending_mtx_);
            if (pending_channels_.empty()) {
                break;
            }
            pending = pending_channels_.front();
            pending_channels_.pop_front();
        }
        if (pending.channel >= 1 && pending.channel <= kChannelCount) {
            model_.SetChannel(pending.channel - 1, pending.on, pending.percent,
                              pending.raw);
        }
        break;
    }

    case kMsgStatus: {
        DeviceStatus status;
        {
            std::lock_guard<std::mutex> lock(pending_mtx_);
            if (pending_statuses_.empty()) {
                break;
            }
            status = pending_statuses_.front();
            pending_statuses_.pop_front();
        }
        model_.status = status;
        break;
    }

    case kMsgLog: {
        std::string text;
        {
            std::lock_guard<std::mutex> lock(pending_mtx_);
            if (pending_logs_.empty()) {
                break;
            }
            text = std::move(pending_logs_.front());
            pending_logs_.pop_front();
        }
        AppendLogEntry(text);
        break;
    }

    case WM_TIMER: {
        // 滑块拖动节流：每 80ms 发送一次当前亮度，松手时由 View 立即发送。
        int index = static_cast<int>(w_param) -
                    static_cast<int>(kTimerSliderBase);
        if (index >= 0 && index < kChannelCount) {
            OnSetChannel(
                index, model_.channels[static_cast<size_t>(index)].percent);
        }
        break;
    }

    case WM_DESTROY:
        SaveConfig();
        logger_.SetFileSink(false, "logs");
        break;

    default:
        return 0;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
    return 0;
}

void Controller::OnPortRefresh() {
    std::vector<std::string> ports = discovery_->GetAllPorts();
    std::string previous;
    if (model_.port_index >= 0 &&
        static_cast<size_t>(model_.port_index) < model_.port_names.size()) {
        previous = model_.port_names[static_cast<size_t>(model_.port_index)];
    } else {
        previous = config_.port_name;
    }
    model_.port_names = std::move(ports);
    model_.port_index = -1;
    for (size_t index = 0; index < model_.port_names.size(); ++index) {
        if (model_.port_names[index] == previous) {
            model_.port_index = static_cast<int>(index);
            break;
        }
    }
}

void Controller::OnConnect() {
    ConnectionState state = model_.connection_state;
    if (state == ConnectionState::Connecting ||
        state == ConnectionState::Reconnecting) {
        return;
    }
    if (model_.connected) {
        device_->DisconnectAsync();
        AppendUILog("正在断开设备连接...");
        return;
    }
    std::string port = "auto";
    if (model_.port_index >= 0 &&
        static_cast<size_t>(model_.port_index) < model_.port_names.size()) {
        port = model_.port_names[static_cast<size_t>(model_.port_index)];
    }
    int baud = kDefaultBaudRate;
    if (model_.baud_index >= 0 &&
        static_cast<size_t>(model_.baud_index) < model_.baud_options.size()) {
        baud = std::stoi(
            model_.baud_options[static_cast<size_t>(model_.baud_index)]);
    }
    AppendUILog("正在连接设备...");
    device_->ConnectAsync(port, baud);
}

void Controller::OnSetChannel(int channel_index, int percent) {
    if (channel_index < 0 || channel_index >= kChannelCount) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    int raw = PercentToRaw(percent);
    model_.SetChannel(channel_index, percent > 0, percent, raw);
    SaveConfig();
    if (!model_.connected) {
        return;
    }
    device_->SetChannelAsync(channel_index + 1, percent);
}

void Controller::OnToggleChannel(int channel_index) {
    if (channel_index < 0 || channel_index >= kChannelCount) {
        return;
    }
    const ChannelState& current =
        model_.channels[static_cast<size_t>(channel_index)];
    if (!model_.connected) {
        AppendUILog("当前未连接设备，无法执行开关灯。");
        return;
    }
    int percent = current.on ? 0 : (current.percent > 0 ? current.percent : 50);
    OnSetChannel(channel_index, percent);
}

void Controller::OnApply() {
    device_->SetMode(model_.mode_index);
    device_->SetPwm(model_.pwm_index);
    device_->SetRestoreStableMode(model_.restore_stable_mode);
    SaveConfig();
    if (!model_.connected) {
        AppendUILog("当前未连接设备，设置仅保留在界面中。");
        return;
    }
    AppendUILog("正在应用调光设置...");
    device_->ApplyAsync();
}

void Controller::OnQueryStatus() {
    if (!model_.connected) {
        AppendUILog("当前未连接设备，无法查询状态。");
        return;
    }
    device_->QueryStatusAsync();
}

void Controller::OnSetPortIndex(int index) {
    model_.port_index = index;
    if (index >= 0 && static_cast<size_t>(index) < model_.port_names.size()) {
        config_.port_name = model_.port_names[static_cast<size_t>(index)];
    }
    SaveConfig();
}

void Controller::OnSetBaudIndex(int index) {
    if (index < 0 || static_cast<size_t>(index) >= model_.baud_options.size()) {
        return;
    }
    model_.baud_index = index;
    config_.baud_rate =
        std::stoi(model_.baud_options[static_cast<size_t>(index)]);
    SaveConfig();
}

void Controller::OnSetModeIndex(int index) {
    if (index < 0 || static_cast<size_t>(index) >= model_.mode_options.size()) {
        return;
    }
    model_.mode_index = index;
    config_.mode = index;
    SaveConfig();
}

void Controller::OnSetPwmIndex(int index) {
    if (index < 0 || static_cast<size_t>(index) >= model_.pwm_options.size()) {
        return;
    }
    model_.pwm_index = index;
    config_.pwm = index;
    SaveConfig();
}

void Controller::OnSetRestoreStableMode(bool enabled) {
    model_.restore_stable_mode = enabled;
    config_.restore_stable_mode = enabled;
    device_->SetRestoreStableMode(enabled);
    SaveConfig();
}

void Controller::AppendUILog(const std::string& text) {
    AppendLogEntry(text);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Controller::AppendLogEntry(const std::string& text) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u  ",
                  static_cast<unsigned>(now.wHour),
                  static_cast<unsigned>(now.wMinute),
                  static_cast<unsigned>(now.wSecond));
    model_.AddLog(std::string(buffer) + text, LogColorFor(text));
}

int Controller::LogColorFor(const std::string& text) const {
    if (text.find("错误") != std::string::npos ||
        text.find("失败") != std::string::npos) {
        return LogColor::Error;
    }
    if (text.find("TX") != std::string::npos) {
        return LogColor::Tx;
    }
    if (text.find("RX") != std::string::npos) {
        return LogColor::Rx;
    }
    return LogColor::Normal;
}

void Controller::SaveConfig() {
    for (int index = 0; index < kChannelCount; ++index) {
        config_.channels[static_cast<size_t>(index)] =
            model_.channels[static_cast<size_t>(index)].percent;
    }
    SaveAppConfig(DefaultConfigPath(), config_);
}

}  // namespace gui
}  // namespace lamp
