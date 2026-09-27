#include "lamp/transport/mock_serial.h"

#include "lamp/core/model.h"

#include <memory>
#include <string>

namespace lamp {

bool MockSerialTransport::Open(const std::string& port_name, int baud_rate) {
    if (!open_result) {
        last_error_ = open_error.empty() ? "Mock open failed." : open_error;
        return false;
    }
    port_name_ = port_name;
    baud_rate_ = baud_rate;
    is_open_ = true;
    last_error_.clear();
    open_count++;
    return true;
}

void MockSerialTransport::Close() {
    is_open_ = false;
    port_name_.clear();
}

bool MockSerialTransport::IsOpen() const {
    return is_open_;
}

bool MockSerialTransport::Exchange(const std::string& command, int read_milliseconds,
                                   std::string& response) {
    (void)read_milliseconds;
    if (!is_open_) {
        last_error_ = "The serial port is not connected.";
        return false;
    }
    exchanged.push_back(command);
    if (fail_exchange_count > 0) {
        fail_exchange_count--;
        last_error_ = "Mock exchange failed.";
        response.clear();
        return false;
    }
    response.clear();
    for (const auto& entry : script) {
        if (command.find(entry.first) != std::string::npos) {
            response = entry.second;
            break;
        }
    }
    last_error_.clear();
    return true;
}

std::string MockSerialTransport::LastError() const {
    return last_error_;
}

std::string MockSerialTransport::PortName() const {
    return port_name_;
}

int MockSerialTransport::BaudRate() const {
    return baud_rate_;
}

std::unique_ptr<MockSerialTransport> CreateMockSerialTransport() {
    return std::make_unique<MockSerialTransport>();
}

}  // namespace lamp
