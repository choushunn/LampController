#include "lamp/config/app_config.h"

#include <cstdlib>
#include <fstream>
#include <string>

#include <windows.h>

namespace lamp {

namespace {

void Trim(std::string& text) {
    size_t begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        text.clear();
        return;
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    text = text.substr(begin, end - begin + 1);
}

int ParseInt(const std::string& value, int fallback) {
    std::string text = value;
    Trim(text);
    if (text.empty()) {
        return fallback;
    }
    char* end = nullptr;
    long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str()) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

}  // namespace

AppConfigData LoadAppConfig(const std::string& path) {
    AppConfigData config;
    std::ifstream file(path);
    if (!file.is_open()) {
        return config;
    }

    std::string line;
    while (std::getline(file, line)) {
        Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }
        size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        Trim(key);
        Trim(value);

        if (key == "port_name") {
            config.port_name = value;
        } else if (key == "baud_rate") {
            config.baud_rate = ParseInt(value, kDefaultBaudRate);
        } else if (key == "mode") {
            config.mode = ParseInt(value, 0);
        } else if (key == "pwm") {
            config.pwm = ParseInt(value, 1);
        } else if (key == "restore_stable_mode") {
            config.restore_stable_mode = ParseInt(value, 1) != 0;
        } else if (key == "channel1") {
            config.channels[0] = ParseInt(value, 0);
        } else if (key == "channel2") {
            config.channels[1] = ParseInt(value, 0);
        } else if (key == "channel3") {
            config.channels[2] = ParseInt(value, 0);
        } else if (key == "channel4") {
            config.channels[3] = ParseInt(value, 0);
        }
    }
    return config;
}

void SaveAppConfig(const std::string& path, const AppConfigData& config) {
    // 确保父目录存在（首次运行 %APPDATA%\LampController 尚不存在，
    // 否则 ofstream 打开失败将静默丢弃全部设置）。
    size_t separator = path.find_last_of("\\/");
    if (separator != std::string::npos && separator > 0) {
        CreateDirectoryA(path.substr(0, separator).c_str(), NULL);
    }
    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        return;
    }
    file << "port_name=" << config.port_name << "\n";
    file << "baud_rate=" << config.baud_rate << "\n";
    file << "mode=" << config.mode << "\n";
    file << "pwm=" << config.pwm << "\n";
    file << "restore_stable_mode=" << (config.restore_stable_mode ? 1 : 0)
         << "\n";
    for (int index = 0; index < kChannelCount; index++) {
        file << "channel" << (index + 1) << "=" << config.channels[index]
             << "\n";
    }
}

std::string DefaultConfigPath() {
    char buffer[MAX_PATH];
    DWORD length = GetEnvironmentVariableA("APPDATA", buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return "config.ini";
    }
    return std::string(buffer) + "\\LampController\\config.ini";
}

}  // namespace lamp
