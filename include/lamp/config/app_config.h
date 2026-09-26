#pragma once

#include "lamp/core/model.h"

#include <array>
#include <string>

namespace lamp {

// Persisted application settings. Channels are brightness percentages.
struct AppConfigData {
    std::string port_name;
    int baud_rate = kDefaultBaudRate;
    int mode = 0;
    int pwm = 1;
    bool restore_stable_mode = true;
    std::array<int, kChannelCount> channels{};
};

// Reads an INI-style key=value file. Missing or malformed entries fall back to
// defaults; a missing file yields fully defaulted data.
AppConfigData LoadAppConfig(const std::string& path);

// Writes the config as INI-style key=value lines. The parent directory must
// already exist.
void SaveAppConfig(const std::string& path, const AppConfigData& config);

// %APPDATA%\LampController\config.ini on Windows, falling back to ./config.ini
// when the environment variable is absent.
std::string DefaultConfigPath();

}  // namespace lamp
