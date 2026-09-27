#pragma once

#include "lamp/transport/serial_transport.h"

#include <string>
#include <utility>
#include <vector>

namespace lamp {

// Scriptable in-memory transport used by tests and smoke runs. Responses are
// matched against exchanged commands by substring; the first matching entry
// wins. When no entry matches, the response is empty.
class MockSerialTransport : public SerialTransport {
public:
    bool open_result = true;
    std::string open_error;
    std::vector<std::pair<std::string, std::string>> script;
    std::vector<std::string> exchanged;
    int fail_exchange_count = 0;
    int open_count = 0;  // Open() 成功调用次数，供重连退避等测试断言。

    bool Open(const std::string& port_name, int baud_rate) override;
    void Close() override;
    bool IsOpen() const override;
    bool Exchange(const std::string& command, int read_milliseconds,
                  std::string& response) override;
    std::string LastError() const override;
    std::string PortName() const override;
    int BaudRate() const override;

private:
    bool is_open_ = false;
    std::string port_name_;
    int baud_rate_;
    std::string last_error_;
};

std::unique_ptr<MockSerialTransport> CreateMockSerialTransport();

}  // namespace lamp
