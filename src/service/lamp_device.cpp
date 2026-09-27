#include "lamp/service/lamp_device.h"

#include "lamp/core/protocol.h"
#include "lamp/core/units.h"
#include "lamp/logging/logger.h"
#include "lamp/platform/port_discovery.h"
#include "lamp/service/connection_state.h"
#include "lamp/transport/serial_transport.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace lamp {

namespace {

std::chrono::seconds ReconnectBackoff(int attempt) {
    if (attempt <= 0) {
        return std::chrono::seconds(1);
    }
    if (attempt >= 3) {
        return std::chrono::seconds(4);
    }
    return std::chrono::seconds(1 << attempt);
}

}  // namespace

struct LampDevice::Impl {
    Impl(std::unique_ptr<SerialTransport> t, std::unique_ptr<PortDiscovery> d,
         Logger& l, DeviceNotifier n, Options o)
        : transport(std::move(t)), discovery(std::move(d)), logger(l),
          notifier(std::move(n)), options(o) {}

    std::unique_ptr<SerialTransport> transport;
    std::unique_ptr<PortDiscovery> discovery;
    Logger& logger;
    DeviceNotifier notifier;
    Options options;

    mutable std::mutex mtx;
    std::condition_variable cv;
    std::deque<std::function<void()>> queue;
    std::thread worker;
    bool quitting = false;

    ConnectionStateMachine state_machine;
    std::array<ChannelState, kChannelCount> channels{};
    std::string port_name;
    int baud_rate = kDefaultBaudRate;
    std::string last_error;
    int mode = 0;
    int pwm = 1;
    bool restore_stable_mode = true;
    bool auto_reconnect = true;

    int reconnect_attempt = 0;
    std::chrono::steady_clock::time_point retry_at{};
    bool reconnect_pending = false;

    void Run();
    bool Post(std::function<void()> op);

    Result DoConnect(const std::string& port, int baud);
    Result DoDisconnect();
    Result DoSetChannel(int channel, int percent);
    Result DoSetChannelRaw(int channel, int raw);
    Result DoSetChannelValue(int channel, int percent, int raw, bool raw_mode);
    Result DoSetChannels(const std::array<int, kChannelCount>& percents);
    Result DoApply();
    Result DoQueryStatus(DeviceStatus& status, bool fatal);
    Result DoSendRaw(const std::string& command, int read_ms,
                     std::string& response);
    Result ExecuteSequence(const std::vector<protocol::Command>& seq);

    void SetError(const std::string& message);
    void ReportLastError();
    void HandleFailure(const std::string& message);
    void ScheduleReconnect();

    void NotifyState(ConnectionState state);
    void NotifyConnected(bool connected);
    void NotifyChannel(int channel);
    void NotifyStatus(const DeviceStatus& status);
    void LogMessage(const std::string& message);
};

void LampDevice::Impl::Run() {
    std::unique_lock<std::mutex> lock(mtx);
    for (;;) {
        if (quitting) {
            break;
        }
        while (!queue.empty()) {
            auto op = std::move(queue.front());
            queue.pop_front();
            lock.unlock();
            op();
            lock.lock();
            if (quitting) {
                break;
            }
        }
        if (quitting) {
            break;
        }
        if (reconnect_pending) {
            auto now = std::chrono::steady_clock::now();
            if (now >= retry_at) {
                std::string port = port_name;
                int baud = baud_rate;
                lock.unlock();
                DoConnect(port, baud);
                lock.lock();
            } else {
                cv.wait_until(lock, retry_at,
                              [&] { return quitting || !queue.empty(); });
            }
            continue;
        }
        cv.wait(lock, [&] { return quitting || !queue.empty() || reconnect_pending; });
    }
}

bool LampDevice::Impl::Post(std::function<void()> op) {
    std::lock_guard<std::mutex> lock(mtx);
    if (quitting) {
        return false;
    }
    queue.push_back(std::move(op));
    cv.notify_all();
    return true;
}

Result LampDevice::Impl::DoConnect(const std::string& port, int baud) {
    bool reconnecting = state_machine.Current() == ConnectionState::Reconnecting;
    if (!reconnecting) {
        state_machine.TryBeginConnect();
        NotifyState(ConnectionState::Connecting);
    }

    int resolved_baud = baud;
    if (!IsValidBaudRate(resolved_baud)) {
        resolved_baud = kDefaultBaudRate;
    }

    std::string resolved = port;
    if (resolved.empty() || resolved == "auto") {
        auto found = discovery->FindPreferredPort();
        if (!found) {
            HandleFailure("未找到唯一串口，请指定端口。");
            return Result::Error;
        }
        resolved = *found;
    }

    transport->Close();
    if (!transport->Open(resolved, resolved_baud)) {
        std::string open_error = transport->LastError();
        SetError(open_error);
        LogMessage(open_error);
        if (reconnecting) {
            ScheduleReconnect();
            return Result::SerialError;
        }
        state_machine.TryFailed();
        NotifyState(ConnectionState::Failed);
        NotifyConnected(false);
        return Result::SerialError;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        port_name = resolved;
        baud_rate = resolved_baud;
        last_error.clear();
        channels = {};
    }
    NotifyConnected(true);

    Result result = ExecuteSequence(protocol::InitializeDeviceSequence());
    if (result != Result::Ok) {
        ReportLastError();
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        reconnect_attempt = 0;
        reconnect_pending = false;
    }
    state_machine.TryConnected();
    NotifyState(ConnectionState::Connected);
    NotifyConnected(true);

    DeviceStatus status;
    DoQueryStatus(status, false);
    return Result::Ok;
}

Result LampDevice::Impl::DoDisconnect() {
    transport->Close();
    {
        std::lock_guard<std::mutex> lock(mtx);
        reconnect_pending = false;
        reconnect_attempt = 0;
    }
    state_machine.TryDisconnect();
    NotifyState(ConnectionState::Disconnected);
    NotifyConnected(false);
    return Result::Ok;
}

Result LampDevice::Impl::DoSetChannel(int channel, int percent) {
    return DoSetChannelValue(channel, percent, PercentToRaw(percent), false);
}

Result LampDevice::Impl::DoSetChannelRaw(int channel, int raw) {
    return DoSetChannelValue(channel, RawToPercent(raw), raw, true);
}

Result LampDevice::Impl::DoSetChannelValue(int channel, int percent, int raw,
                                           bool raw_mode) {
    if (!IsValidChannel(channel)) {
        SetError("通道号必须是 1-4。");
        return Result::InvalidArgument;
    }
    if ((raw_mode && !IsValidRaw(raw)) || (!raw_mode && !IsValidPercent(percent))) {
        SetError(raw_mode ? "亮度原始值必须是 0-255。"
                          : "亮度百分比必须是 0-100。");
        return Result::InvalidArgument;
    }
    if (!transport->IsOpen()) {
        HandleFailure("串口未连接。");
        return Result::NotConnected;
    }

    bool restore_stable = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        restore_stable = restore_stable_mode;
    }
    Result result = ExecuteSequence(
        protocol::SetChannelRawSequence(channel, raw, restore_stable));
    if (result != Result::Ok) {
        ReportLastError();
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        channels[channel - 1] = {raw > 0, percent, raw};
    }
    NotifyChannel(channel);
    if (raw_mode) {
        LogMessage("灯 " + std::to_string(channel) + " 亮度原始值已设置为 " +
                   std::to_string(raw) + "。");
    } else {
        LogMessage("灯 " + std::to_string(channel) + " 亮度已设置为 " +
                   std::to_string(percent) + "%。");
    }
    return Result::Ok;
}

Result LampDevice::Impl::DoSetChannels(const std::array<int, kChannelCount>& percents) {
    for (int index = 0; index < kChannelCount; index++) {
        if (!IsValidPercent(percents[index])) {
            SetError("亮度百分比必须是 0-100。");
            return Result::InvalidArgument;
        }
    }
    if (!transport->IsOpen()) {
        HandleFailure("串口未连接。");
        return Result::NotConnected;
    }

    std::vector<int> raws;
    for (int percent : percents) {
        raws.push_back(PercentToRaw(percent));
    }
    bool restore_stable = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        restore_stable = restore_stable_mode;
    }
    Result result =
        ExecuteSequence(protocol::SetChannelsRawSequence(raws, restore_stable));
    if (result != Result::Ok) {
        ReportLastError();
        return result;
    }

    {
        std::lock_guard<std::mutex> lock(mtx);
        for (int index = 0; index < kChannelCount; index++) {
            channels[index] = {percents[index] > 0, percents[index], raws[index]};
        }
    }
    for (int channel = 1; channel <= kChannelCount; channel++) {
        NotifyChannel(channel);
    }
    return Result::Ok;
}

Result LampDevice::Impl::DoApply() {
    if (!transport->IsOpen()) {
        HandleFailure("串口未连接。");
        return Result::NotConnected;
    }

    int apply_mode = 0;
    int apply_pwm = 1;
    bool restore_stable = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        apply_mode = mode;
        apply_pwm = pwm;
        restore_stable = restore_stable_mode;
    }

    std::vector<protocol::Command> sequence;
    sequence.push_back({protocol::CommandType::SetMode, 0, apply_mode, "", 220});
    sequence.push_back({protocol::CommandType::SetPwmFrequency, 0, apply_pwm, "", 220});
    for (int index = 0; index < kChannelCount; index++) {
        int raw = 0;
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (channels[index].on) {
                raw = channels[index].raw;
            }
        }
        sequence.push_back({protocol::CommandType::SetChannel, index + 1, raw, "", 220});
    }
    sequence.push_back({protocol::CommandType::EnableOutput, 0, 1, "", 220});
    auto apply = protocol::ApplySequence(restore_stable && apply_mode == 0);
    sequence.insert(sequence.end(), apply.begin(), apply.end());

    Result result = ExecuteSequence(sequence);
    if (result != Result::Ok) {
        ReportLastError();
        return result;
    }
    for (int channel = 1; channel <= kChannelCount; channel++) {
        NotifyChannel(channel);
    }
    return Result::Ok;
}

Result LampDevice::Impl::DoQueryStatus(DeviceStatus& status, bool fatal) {
    if (!transport->IsOpen()) {
        HandleFailure("串口未连接。");
        return Result::NotConnected;
    }

    DeviceStatus parsed;
    for (const auto& command : protocol::QueryStatusSequence()) {
        std::string line = protocol::Build(command);
        std::string response;
        LogMessage("TX " + line);
        if (!transport->Exchange(line, command.read_ms, response)) {
            std::string error = transport->LastError();
            SetError(error);
            LogMessage("TX " + line + " 失败：" + error);
            if (fatal) {
                HandleFailure(error);
            }
            return Result::SerialError;
        }
        if (!response.empty()) {
            LogMessage("RX " + response);
        }
        if (command.key == "VER") {
            parsed.version = protocol::ParseStatusValue(response, "VER").value_or(-1);
        } else if (command.key == "MOD") {
            parsed.mode = protocol::ParseStatusValue(response, "MOD").value_or(-1);
        } else if (command.key == "PWMP") {
            parsed.pwm = protocol::ParseStatusValue(response, "PWMP").value_or(-1);
        } else if (command.key == "CTRL") {
            parsed.control = protocol::ParseStatusValue(response, "CTRL").value_or(-1);
        } else if (command.key == "TEMP") {
            parsed.temperature = protocol::ParseStatusValue(response, "TEMP").value_or(-1);
        } else if (command.key == "VOLT") {
            parsed.voltage_raw = protocol::ParseStatusValue(response, "VOLT").value_or(-1);
        } else if (command.key == "CRV") {
            auto value = protocol::ParseStatusValue(response, "CRV");
            if (!value) {
                value = protocol::ParseStatusValue(response, "CUR");
            }
            parsed.current_raw = value.value_or(-1);
        } else if (command.key == "ERRS") {
            parsed.error_flags = protocol::ParseStatusValue(response, "ERRS").value_or(-1);
        }
    }

    status = parsed;
    NotifyStatus(parsed);
    return Result::Ok;
}

Result LampDevice::Impl::DoSendRaw(const std::string& command, int read_ms,
                                   std::string& response) {
    if (!transport->IsOpen()) {
        HandleFailure("串口未连接。");
        return Result::NotConnected;
    }

    LogMessage("TX " + command);
    if (!transport->Exchange(command, read_ms, response)) {
        std::string error = transport->LastError();
        SetError(error);
        LogMessage("TX " + command + " 失败：" + error);
        HandleFailure(error);
        return Result::SerialError;
    }
    if (!response.empty()) {
        LogMessage("RX " + response);
    }
    return Result::Ok;
}

Result LampDevice::Impl::ExecuteSequence(const std::vector<protocol::Command>& seq) {
    for (const auto& command : seq) {
        std::string line = protocol::Build(command);
        std::string response;
        LogMessage("TX " + line);
        if (!transport->Exchange(line, command.read_ms, response)) {
            std::string error = transport->LastError();
            SetError(error);
            LogMessage("TX " + line + " 失败：" + error);
            return Result::SerialError;
        }
        if (!response.empty()) {
            LogMessage("RX " + response);
        }
    }
    return Result::Ok;
}

void LampDevice::Impl::SetError(const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx);
    last_error = message;
}

void LampDevice::Impl::ReportLastError() {
    std::string message;
    {
        std::lock_guard<std::mutex> lock(mtx);
        message = last_error;
    }
    HandleFailure(message);
}

void LampDevice::Impl::HandleFailure(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        last_error = message;
    }
    LogMessage("错误：" + message);
    ConnectionState current = state_machine.Current();
    bool reconnect_enabled = false;
    {
        std::lock_guard<std::mutex> lock(mtx);
        reconnect_enabled = auto_reconnect;
    }
    if (current == ConnectionState::Connected) {
        if (reconnect_enabled) {
            state_machine.TryReconnect();
            NotifyState(ConnectionState::Reconnecting);
            NotifyConnected(false);
            ScheduleReconnect();
        } else {
            state_machine.TryDisconnect();
            state_machine.TryFailed();
            NotifyState(ConnectionState::Failed);
            NotifyConnected(false);
        }
        return;
    }
    if (current == ConnectionState::Connecting) {
        state_machine.TryFailed();
        NotifyState(ConnectionState::Failed);
        NotifyConnected(false);
        return;
    }
    // 重连期间（端口已打开但初始化序列失败等）也要延续指数退避；
    // 否则 reconnect_pending 保持为真且 retry_at 已过期，Run 循环会立即
    // 重试，形成无退避、无重试次数增长的紧循环。
    if (current == ConnectionState::Reconnecting) {
        NotifyConnected(false);
        ScheduleReconnect();
    }
}

void LampDevice::Impl::ScheduleReconnect() {
    {
        std::lock_guard<std::mutex> lock(mtx);
        reconnect_attempt++;
        reconnect_pending = true;
        retry_at = std::chrono::steady_clock::now() + ReconnectBackoff(reconnect_attempt);
    }
    cv.notify_all();
}

void LampDevice::Impl::NotifyState(ConnectionState state) {
    if (notifier.on_state) {
        notifier.on_state(state);
    }
}

void LampDevice::Impl::NotifyConnected(bool connected) {
    if (notifier.on_connected) {
        notifier.on_connected(connected);
    }
}

void LampDevice::Impl::NotifyChannel(int channel) {
    ChannelState state;
    {
        std::lock_guard<std::mutex> lock(mtx);
        state = channels[channel - 1];
    }
    if (notifier.on_channel) {
        notifier.on_channel(channel, state);
    }
}

void LampDevice::Impl::NotifyStatus(const DeviceStatus& status) {
    if (notifier.on_status) {
        notifier.on_status(status);
    }
}

void LampDevice::Impl::LogMessage(const std::string& message) {
    logger.Info(message);
    if (notifier.on_log) {
        notifier.on_log(message);
    }
}

Result LampDevice::SyncCall(std::function<Result()> op) {
    std::promise<Result> promise;
    auto future = promise.get_future();
    if (!impl_->Post([&promise, op = std::move(op)]() mutable {
            promise.set_value(op());
        })) {
        return Result::Error;
    }
    return future.get();
}

LampDevice::LampDevice(std::unique_ptr<SerialTransport> transport,
                       std::unique_ptr<PortDiscovery> discovery, Logger& logger,
                       DeviceNotifier notifier)
    : LampDevice(std::move(transport), std::move(discovery), logger,
                 std::move(notifier), Options()) {}

LampDevice::LampDevice(std::unique_ptr<SerialTransport> transport,
                       std::unique_ptr<PortDiscovery> discovery, Logger& logger,
                       DeviceNotifier notifier, Options options)
    : impl_(new Impl(std::move(transport), std::move(discovery), logger,
                     std::move(notifier), options)) {
    impl_->worker = std::thread(&Impl::Run, impl_.get());
}

LampDevice::~LampDevice() {
    {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->quitting = true;
    }
    impl_->cv.notify_all();
    if (impl_->worker.joinable()) {
        impl_->worker.join();
    }
    impl_->transport->Close();
}

void LampDevice::ConnectAsync(const std::string& port_name, int baud_rate) {
    impl_->Post([this, port_name, baud_rate] { impl_->DoConnect(port_name, baud_rate); });
}

void LampDevice::DisconnectAsync() {
    impl_->Post([this] { impl_->DoDisconnect(); });
}

void LampDevice::SetChannelAsync(int channel, int percent) {
    impl_->Post([this, channel, percent] { impl_->DoSetChannel(channel, percent); });
}

void LampDevice::SetChannelRawAsync(int channel, int raw) {
    impl_->Post([this, channel, raw] { impl_->DoSetChannelRaw(channel, raw); });
}

void LampDevice::SetChannelsAsync(const std::array<int, kChannelCount>& percents) {
    impl_->Post([this, percents] { impl_->DoSetChannels(percents); });
}

void LampDevice::ApplyAsync() {
    impl_->Post([this] { impl_->DoApply(); });
}

void LampDevice::QueryStatusAsync() {
    impl_->Post([this] {
        DeviceStatus status;
        impl_->DoQueryStatus(status, false);
    });
}

Result LampDevice::Connect(const std::string& port_name, int baud_rate) {
    return SyncCall([this, port_name, baud_rate] {
        return impl_->DoConnect(port_name, baud_rate);
    });
}

Result LampDevice::Disconnect() {
    return SyncCall([this] { return impl_->DoDisconnect(); });
}

Result LampDevice::SetChannel(int channel, int percent) {
    return SyncCall([this, channel, percent] {
        return impl_->DoSetChannel(channel, percent);
    });
}

Result LampDevice::SetChannelRaw(int channel, int raw) {
    return SyncCall([this, channel, raw] {
        return impl_->DoSetChannelRaw(channel, raw);
    });
}

Result LampDevice::SetChannels(const std::array<int, kChannelCount>& percents) {
    return SyncCall([this, percents] { return impl_->DoSetChannels(percents); });
}

Result LampDevice::Apply() {
    return SyncCall([this] { return impl_->DoApply(); });
}

Result LampDevice::QueryStatus(DeviceStatus& status) {
    return SyncCall([this, &status] {
        return impl_->DoQueryStatus(status, true);
    });
}

Result LampDevice::SendRaw(const std::string& command, int read_ms,
                           std::string& response) {
    return SyncCall([this, command, read_ms, &response] {
        return impl_->DoSendRaw(command, read_ms, response);
    });
}

void LampDevice::SetMode(int value) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->mode = value;
}

void LampDevice::SetPwm(int value) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->pwm = value;
}

void LampDevice::SetRestoreStableMode(bool enabled) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->restore_stable_mode = enabled;
}

void LampDevice::SetAutoReconnect(bool enabled) {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    impl_->auto_reconnect = enabled;
}

ConnectionState LampDevice::State() const {
    return impl_->state_machine.Current();
}

bool LampDevice::IsConnected() const {
    return State() == ConnectionState::Connected;
}

std::string LampDevice::LastError() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->last_error;
}

std::string LampDevice::PortName() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->port_name;
}

int LampDevice::BaudRate() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->baud_rate;
}

int LampDevice::Mode() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->mode;
}

int LampDevice::Pwm() const {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    return impl_->pwm;
}

}  // namespace lamp
