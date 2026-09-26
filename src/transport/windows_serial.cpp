#include "lamp/transport/serial_transport.h"

#include "lamp/core/model.h"
#include "lamp/core/units.h"

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <string>

namespace lamp {

namespace {

bool normalize_command(const std::string& command, std::string& normalized) {
    size_t begin = 0;
    while (begin < command.size() &&
           (command[begin] == ' ' || command[begin] == '\t')) {
        begin++;
    }
    size_t end = command.size();
    while (end > begin && (command[end - 1] == ' ' || command[end - 1] == '\t')) {
        end--;
    }
    if (end <= begin) {
        return false;
    }
    normalized = command.substr(begin, end - begin);
    if (normalized.back() != '#') {
        normalized.push_back('#');
    }
    return normalized.size() <= 256;
}

std::string format_open_error(const std::string& port, DWORD error_code) {
    char buffer[kErrorSize];
    std::snprintf(buffer, sizeof(buffer), "Could not open %s (Windows error %lu).",
                  port.c_str(), static_cast<unsigned long>(error_code));
    return buffer;
}

std::string format_win_error(const char* prefix, DWORD error_code) {
    char buffer[kErrorSize];
    std::snprintf(buffer, sizeof(buffer), "%s (Windows error %lu).", prefix,
                  static_cast<unsigned long>(error_code));
    return buffer;
}

}  // namespace

class WindowsSerialTransport : public SerialTransport {
public:
    WindowsSerialTransport() = default;
    ~WindowsSerialTransport() override {
        Close();
    }

    bool Open(const std::string& port_name, int baud_rate) override {
        Close();
        last_error_.clear();

        if (!IsValidBaudRate(baud_rate)) {
            baud_rate = kDefaultBaudRate;
        }

        std::string resolved = port_name;
        if (resolved.empty()) {
            last_error_ = "No serial port name was provided.";
            return false;
        }
        for (char& c : resolved) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        std::string device_path = "\\\\.\\" + resolved;
        handle_ = CreateFileA(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                              NULL, OPEN_EXISTING, 0, NULL);
        if (handle_ == INVALID_HANDLE_VALUE) {
            last_error_ = format_open_error(resolved, GetLastError());
            return false;
        }

        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);
        if (!GetCommState(handle_, &dcb)) {
            last_error_ = format_win_error("GetCommState failed", GetLastError());
            Close();
            return false;
        }

        dcb.BaudRate = static_cast<DWORD>(baud_rate);
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;
        dcb.fDtrControl = DTR_CONTROL_DISABLE;
        dcb.fDsrSensitivity = FALSE;
        dcb.fTXContinueOnXoff = TRUE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fErrorChar = FALSE;
        dcb.fNull = FALSE;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fAbortOnError = FALSE;

        if (!SetCommState(handle_, &dcb)) {
            last_error_ = format_win_error("SetCommState failed", GetLastError());
            Close();
            return false;
        }

        COMMTIMEOUTS timeouts;
        memset(&timeouts, 0, sizeof(timeouts));
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 700;
        if (!SetCommTimeouts(handle_, &timeouts)) {
            last_error_ = format_win_error("SetCommTimeouts failed", GetLastError());
            Close();
            return false;
        }

        PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
        Sleep(120);
        PurgeComm(handle_, PURGE_RXCLEAR);

        port_name_ = resolved;
        baud_rate_ = baud_rate;
        return true;
    }

    void Close() override {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = INVALID_HANDLE_VALUE;
        port_name_.clear();
    }

    bool IsOpen() const override {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    bool Exchange(const std::string& command, int read_milliseconds,
                  std::string& response) override {
        response.clear();
        if (!IsOpen()) {
            last_error_ = "The serial port is not connected.";
            return false;
        }

        std::string normalized;
        if (!normalize_command(command, normalized)) {
            last_error_ = "The command is empty or too long.";
            return false;
        }

        int read_ms = read_milliseconds < 30 ? 30
                     : read_milliseconds > 10000 ? 10000
                                                 : read_milliseconds;

        PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
        DWORD written = 0;
        if (!WriteFile(handle_, normalized.c_str(),
                       static_cast<DWORD>(normalized.size()), &written, NULL) ||
            written != static_cast<DWORD>(normalized.size())) {
            last_error_ = format_win_error("Serial write failed", GetLastError());
            return false;
        }

        Sleep(static_cast<DWORD>(read_ms));
        DWORD started = GetTickCount();
        std::string buffer;
        buffer.reserve(2048);

        while (buffer.size() + 1 < 2048) {
            COMSTAT communications_status;
            DWORD errors = 0;
            DWORD available = 0;
            if (!ClearCommError(handle_, &errors, &communications_status)) {
                break;
            }
            available = communications_status.cbInQue;
            if (available == 0) {
                if (GetTickCount() - started > 50) {
                    break;
                }
                Sleep(5);
                continue;
            }

            DWORD to_read = available > 2047 - buffer.size()
                                ? 2047 - static_cast<DWORD>(buffer.size())
                                : available;
            char chunk[256];
            DWORD received = 0;
            if (!ReadFile(handle_, chunk, to_read, &received, NULL) ||
                received == 0) {
                break;
            }
            buffer.append(chunk, received);
        }

        response = buffer;
        return true;
    }

    std::string LastError() const override {
        return last_error_;
    }

    std::string PortName() const override {
        return port_name_;
    }

    int BaudRate() const override {
        return baud_rate_;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::string port_name_;
    int baud_rate_ = kDefaultBaudRate;
    std::string last_error_;
};

std::unique_ptr<SerialTransport> CreateWindowsSerialTransport() {
    return std::make_unique<WindowsSerialTransport>();
}

}  // namespace lamp
