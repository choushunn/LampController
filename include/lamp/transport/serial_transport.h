#pragma once

#include <memory>
#include <string>

namespace lamp {

// Transport abstraction used by the service layer. Implementations are
// expected to be used from a single thread at a time.
class SerialTransport {
public:
    virtual ~SerialTransport() = default;

    virtual bool Open(const std::string& port_name, int baud_rate) = 0;
    virtual void Close() = 0;
    virtual bool IsOpen() const = 0;

    // Sends a text command and reads the response until the device goes idle.
    virtual bool Exchange(const std::string& command, int read_milliseconds,
                          std::string& response) = 0;

    virtual std::string LastError() const = 0;
    virtual std::string PortName() const = 0;
    virtual int BaudRate() const = 0;
};

std::unique_ptr<SerialTransport> CreateWindowsSerialTransport();

}  // namespace lamp
