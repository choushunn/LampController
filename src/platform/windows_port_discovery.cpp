#include "lamp/platform/port_discovery.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace lamp {

namespace {

bool is_preferred_keyword(const std::string& value_name) {
    std::string lower;
    lower.reserve(value_name.size());
    for (char c : value_name) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lower.find("usb") != std::string::npos ||
           lower.find("ch340") != std::string::npos ||
           lower.find("cp210") != std::string::npos ||
           lower.find("prolific") != std::string::npos;
}

int compare_ports(const std::string& left, const std::string& right) {
    std::string a;
    std::string b;
    a.reserve(left.size());
    b.reserve(right.size());
    for (char c : left) {
        a.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    for (char c : right) {
        b.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

class WindowsPortDiscovery : public PortDiscovery {
public:
    std::vector<std::string> GetAllPorts() override {
        std::vector<std::string> ports;
        HKEY key = NULL;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0,
                          KEY_READ, &key) != ERROR_SUCCESS) {
            return ports;
        }
        DWORD value_count = 0;
        DWORD maximum_value_name = 0;
        DWORD maximum_value_data = 0;
        if (RegQueryInfoKeyA(key, NULL, NULL, NULL, NULL, NULL, NULL, &value_count,
                             &maximum_value_name, &maximum_value_data, NULL,
                             NULL) != ERROR_SUCCESS) {
            RegCloseKey(key);
            return ports;
        }
        if (maximum_value_name < 64) {
            maximum_value_name = 64;
        }
        if (maximum_value_data < 64) {
            maximum_value_data = 64;
        }
        for (DWORD index = 0; index < value_count; index++) {
            std::vector<char> value_name(maximum_value_name + 1);
            std::vector<char> value_data(maximum_value_data + 1);
            DWORD value_name_size = static_cast<DWORD>(value_name.size());
            DWORD value_data_size = static_cast<DWORD>(value_data.size());
            DWORD type = 0;
            if (RegEnumValueA(key, index, value_name.data(), &value_name_size, NULL,
                              &type, reinterpret_cast<BYTE*>(value_data.data()),
                              &value_data_size) != ERROR_SUCCESS ||
                type != REG_SZ || value_data_size == 0) {
                continue;
            }
            value_data[value_data.size() - 1] = '\0';
            ports.push_back(value_data.data());
        }
        RegCloseKey(key);
        std::sort(ports.begin(), ports.end(), [](const std::string& a, const std::string& b) {
            return compare_ports(a, b) < 0;
        });
        return ports;
    }

    std::vector<std::string> GetPreferredPorts() override {
        std::vector<std::string> preferred;
        HKEY key = NULL;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM", 0,
                          KEY_READ, &key) != ERROR_SUCCESS) {
            return preferred;
        }
        DWORD value_count = 0;
        DWORD maximum_value_name = 0;
        DWORD maximum_value_data = 0;
        if (RegQueryInfoKeyA(key, NULL, NULL, NULL, NULL, NULL, NULL, &value_count,
                             &maximum_value_name, &maximum_value_data, NULL,
                             NULL) != ERROR_SUCCESS) {
            RegCloseKey(key);
            return preferred;
        }
        if (maximum_value_name < 64) {
            maximum_value_name = 64;
        }
        if (maximum_value_data < 64) {
            maximum_value_data = 64;
        }
        for (DWORD index = 0; index < value_count; index++) {
            std::vector<char> value_name(maximum_value_name + 1);
            std::vector<char> value_data(maximum_value_data + 1);
            DWORD value_name_size = static_cast<DWORD>(value_name.size());
            DWORD value_data_size = static_cast<DWORD>(value_data.size());
            DWORD type = 0;
            if (RegEnumValueA(key, index, value_name.data(), &value_name_size, NULL,
                              &type, reinterpret_cast<BYTE*>(value_data.data()),
                              &value_data_size) != ERROR_SUCCESS ||
                type != REG_SZ || value_data_size == 0) {
                continue;
            }
            value_data[value_data.size() - 1] = '\0';
            if (is_preferred_keyword(value_name.data())) {
                preferred.push_back(value_data.data());
            }
        }
        RegCloseKey(key);
        std::sort(preferred.begin(), preferred.end(), [](const std::string& a, const std::string& b) {
            return compare_ports(a, b) < 0;
        });
        return preferred;
    }

    std::optional<std::string> FindPreferredPort() override {
        return PickPreferredPort(GetAllPorts(), GetPreferredPorts());
    }
};

}  // namespace

std::optional<std::string> PickPreferredPort(
    const std::vector<std::string>& all_ports,
    const std::vector<std::string>& preferred_ports) {
    if (preferred_ports.size() == 1) {
        return preferred_ports.front();
    }
    if (preferred_ports.empty() && all_ports.size() == 1) {
        return all_ports.front();
    }
    return std::nullopt;
}

std::unique_ptr<PortDiscovery> CreateWindowsPortDiscovery() {
    return std::make_unique<WindowsPortDiscovery>();
}

}  // namespace lamp
