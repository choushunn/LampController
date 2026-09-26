#include "test_harness.h"

#include "lamp/core/model.h"
#include "lamp/logging/logger.h"
#include "lamp/platform/port_discovery.h"
#include "lamp/service/lamp_device.h"
#include "lamp/transport/mock_serial.h"

#include <array>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace lamp;

namespace {

struct StubPortDiscovery : public PortDiscovery {
    std::vector<std::string> all;
    std::vector<std::string> preferred;

    std::vector<std::string> GetAllPorts() override {
        return all;
    }
    std::vector<std::string> GetPreferredPorts() override {
        return preferred;
    }
    std::optional<std::string> FindPreferredPort() override {
        return PickPreferredPort(all, preferred);
    }
};

struct DeviceHarness {
    MockSerialTransport* transport = nullptr;
    StubPortDiscovery* discovery = nullptr;
    Logger logger{LogLevel::Error};
    std::unique_ptr<LampDevice> device;

    bool connected_seen = false;
    ConnectionState last_state = ConnectionState::Disconnected;
    std::vector<std::pair<int, ChannelState>> channel_notices;

    explicit DeviceHarness(bool auto_reconnect = true) {
        auto serial = CreateMockSerialTransport();
        transport = serial.get();
        auto ports = std::make_unique<StubPortDiscovery>();
        discovery = ports.get();
        discovery->all = {"COM9"};
        discovery->preferred = {"COM9"};

        DeviceNotifier notifier;
        notifier.on_state = [this](ConnectionState state) {
            last_state = state;
        };
        notifier.on_connected = [this](bool connected) {
            connected_seen = connected;
        };
        notifier.on_channel = [this](int channel, const ChannelState& state) {
            channel_notices.push_back({channel, state});
        };

        device = std::make_unique<LampDevice>(
            std::move(serial), std::move(ports), logger, std::move(notifier),
            LampDevice::Options{auto_reconnect});
    }
};

bool ExchangedContains(const MockSerialTransport& transport,
                       const std::string& command) {
    for (const auto& item : transport.exchanged) {
        if (item == command) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(device_connect_success) {
    DeviceHarness harness;
    EXPECT_EQ(harness.device->Connect("COM9", 19200), Result::Ok);
    EXPECT_EQ(harness.device->State(), ConnectionState::Connected);
    EXPECT_TRUE(harness.device->IsConnected());
    EXPECT_EQ(harness.device->PortName(), std::string("COM9"));
    EXPECT_EQ(harness.device->BaudRate(), 19200);
    EXPECT_TRUE(harness.connected_seen);
    EXPECT_EQ(harness.last_state, ConnectionState::Connected);
}

TEST(device_connect_open_failure) {
    DeviceHarness harness;
    harness.transport->open_result = false;
    harness.transport->open_error = "COM9 in use";
    EXPECT_EQ(harness.device->Connect("COM9", 19200), Result::SerialError);
    EXPECT_EQ(harness.device->State(), ConnectionState::Failed);
    EXPECT_FALSE(harness.device->IsConnected());
    EXPECT_FALSE(harness.transport->IsOpen());
}

TEST(device_connect_no_port) {
    DeviceHarness harness;
    harness.discovery->all = {};
    harness.discovery->preferred = {};
    EXPECT_EQ(harness.device->Connect("auto", 19200), Result::Error);
    EXPECT_EQ(harness.device->State(), ConnectionState::Failed);
    EXPECT_FALSE(harness.transport->IsOpen());
}

TEST(device_set_channel) {
    DeviceHarness harness;
    harness.device->Connect("COM9", 19200);
    harness.channel_notices.clear();
    EXPECT_EQ(harness.device->SetChannel(2, 50), Result::Ok);
    EXPECT_TRUE(ExchangedContains(*harness.transport, "SPB128#"));
    EXPECT_EQ(harness.channel_notices.size(), size_t(1));
    if (harness.channel_notices.size() == 1) {
        EXPECT_EQ(harness.channel_notices[0].first, 2);
        EXPECT_TRUE(harness.channel_notices[0].second.on);
        EXPECT_EQ(harness.channel_notices[0].second.percent, 50);
        EXPECT_EQ(harness.channel_notices[0].second.raw, 128);
    }
}

TEST(device_set_channel_invalid_argument) {
    DeviceHarness harness;
    harness.device->Connect("COM9", 19200);
    EXPECT_EQ(harness.device->SetChannel(5, 50), Result::InvalidArgument);
    EXPECT_EQ(harness.device->SetChannel(1, 150), Result::InvalidArgument);
}

TEST(device_operation_when_not_connected) {
    DeviceHarness harness;
    EXPECT_EQ(harness.device->SetChannel(1, 50), Result::NotConnected);
    harness.device->QueryStatusAsync();
}

TEST(device_query_status) {
    DeviceHarness harness;
    harness.transport->script = {
        {"S_VER:#", "VER:7#"},
        {"S_MOD:#", "MOD:0#"},
        {"S_PWMP:#", "PWMP:1#"},
        {"S_CTRL:#", "CTRL:1#"},
        {"S_TEMP:#", "TEMP:25#"},
        {"S_VOLT:#", "VOLT:512#"},
        {"S_CRV:#", "CRV:300#"},
        {"S_ERRS:#", "ERRS:0#"},
    };
    harness.device->Connect("COM9", 19200);

    DeviceStatus status;
    EXPECT_EQ(harness.device->QueryStatus(status), Result::Ok);
    EXPECT_EQ(status.version, 7);
    EXPECT_EQ(status.mode, 0);
    EXPECT_EQ(status.pwm, 1);
    EXPECT_EQ(status.control, 1);
    EXPECT_EQ(status.temperature, 25);
    EXPECT_EQ(status.voltage_raw, 512);
    EXPECT_EQ(status.current_raw, 300);
    EXPECT_EQ(status.error_flags, 0);
}

TEST(device_auto_reconnect) {
    DeviceHarness harness;
    EXPECT_EQ(harness.device->Connect("COM9", 19200), Result::Ok);
    EXPECT_EQ(harness.device->State(), ConnectionState::Connected);

    // The next exchange fails, so the device drops to Reconnecting and the
    // worker schedules a reconnect with backoff.
    harness.transport->fail_exchange_count = 1;
    EXPECT_EQ(harness.device->SetChannel(1, 50), Result::SerialError);
    EXPECT_EQ(harness.device->State(), ConnectionState::Reconnecting);

    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (harness.device->State() != ConnectionState::Connected &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT_EQ(harness.device->State(), ConnectionState::Connected);
    EXPECT_TRUE(harness.device->IsConnected());
}

TEST(device_disconnect) {
    DeviceHarness harness;
    harness.device->Connect("COM9", 19200);
    EXPECT_EQ(harness.device->Disconnect(), Result::Ok);
    EXPECT_EQ(harness.device->State(), ConnectionState::Disconnected);
    EXPECT_FALSE(harness.device->IsConnected());
    EXPECT_FALSE(harness.transport->IsOpen());
}
