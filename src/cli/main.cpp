#include "lamp/core/model.h"
#include "lamp/core/units.h"
#include "lamp/logging/logger.h"
#include "lamp/platform/port_discovery.h"
#include "lamp/service/lamp_device.h"
#include "lamp/transport/serial_transport.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

namespace {

using namespace lamp;

struct CliOptions {
    std::string port = "auto";
    int baud = kDefaultBaudRate;
    bool json = false;
    std::vector<int> lights = {1, 2, 3, 4};
    bool has_brightness = false;
    int brightness = 60;
    bool use_raw = false;
    int raw_value = 0;
    bool has_value = false;
    int value = 0;
    bool has_raw_value = false;
    std::string command;
    int read_ms = 300;
    std::array<int, kChannelCount> light_values{};
    bool has_light_values = false;
};

const char* OptionValue(int argc, char** argv, const char* name) {
    size_t length = std::strlen(name);
    for (int index = 2; index < argc; index++) {
        if (std::strcmp(argv[index], name) == 0 && index + 1 < argc) {
            return argv[index + 1];
        }
        if (std::strncmp(argv[index], name, length) == 0 &&
            argv[index][length] == '=') {
            return argv[index] + length + 1;
        }
    }
    return nullptr;
}

bool OptionPresent(int argc, char** argv, const char* name) {
    for (int index = 2; index < argc; index++) {
        if (std::strcmp(argv[index], name) == 0) {
            return true;
        }
    }
    return false;
}

bool ParseInteger(const char* text, int* value) {
    if (text == nullptr || value == nullptr) {
        return false;
    }
    char* end = nullptr;
    long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool ParseLightList(const char* text, std::vector<int>* lights) {
    if (text == nullptr || _stricmp(text, "all") == 0 ||
        _stricmp(text, "both") == 0) {
        *lights = {1, 2, 3, 4};
        return true;
    }
    lights->clear();
    std::string token;
    const std::string list(text);
    for (size_t index = 0; index <= list.size(); index++) {
        if (index == list.size() || list[index] == ',') {
            if (!token.empty()) {
                int channel = 0;
                if (!ParseInteger(token.c_str(), &channel) ||
                    !IsValidChannel(channel)) {
                    return false;
                }
                bool duplicate = false;
                for (int existing : *lights) {
                    if (existing == channel) {
                        duplicate = true;
                    }
                }
                if (!duplicate) {
                    lights->push_back(channel);
                }
            }
            token.clear();
        } else {
            token += list[index];
        }
    }
    return !lights->empty();
}

// Parses --light1..--light4 options. Returns true when at least one is given.
bool ParseLightValues(int argc, char** argv, CliOptions* options) {
    for (int channel = 1; channel <= kChannelCount; channel++) {
        std::string name = "--light" + std::to_string(channel);
        const char* text = OptionValue(argc, argv, name.c_str());
        if (text != nullptr) {
            int value = 0;
            if (!ParseInteger(text, &value) || value < 0 || value > 100) {
                std::fprintf(stderr,
                             "错误：%s 必须是 0-100 的整数。\n", name.c_str());
                return false;
            }
            options->light_values[channel - 1] = value;
            options->has_light_values = true;
        }
    }
    return true;
}

std::unique_ptr<lamp::LampDevice> CreateDevice(lamp::Logger* logger) {
    auto transport = lamp::CreateWindowsSerialTransport();
    auto discovery = lamp::CreateWindowsPortDiscovery();
    lamp::DeviceNotifier notifier;
    return std::make_unique<lamp::LampDevice>(
        std::move(transport), std::move(discovery), *logger,
        std::move(notifier), lamp::LampDevice::Options{false});
}

bool OpenDevice(lamp::LampDevice* device, const CliOptions& options) {
    lamp::Result result = device->Connect(options.port, options.baud);
    if (result != lamp::Result::Ok) {
        std::fprintf(stderr, "错误：%s\n", device->LastError().c_str());
        return false;
    }
    return true;
}

void PrintHelp() {
    std::printf("lampctl - 智能灯光控制器 CLI\n");
    std::printf("\n");
    std::printf("用法：\n");
    std::printf("  lampctl ports [--json]\n");
    std::printf("  lampctl status [--port COM9] [--json]\n");
    std::printf("  lampctl stable [--port COM9]\n");
    std::printf("  lampctl on [--port COM9] [--light 1|2|3|4|all] [--brightness 60]\n");
    std::printf("  lampctl off [--port COM9] [--light 1|2|3|4|all]\n");
    std::printf("  lampctl brightness [--port COM9] [--light 1|2|3|4|all] --value 60\n");
    std::printf("  lampctl brightness [--port COM9] [--light 1|2|3|4|all] --raw 153\n");
    std::printf("  lampctl set [--port COM9] --light1 60 --light2 30 --light3 40 --light4 50\n");
    std::printf("  lampctl raw [--port COM9] --command \"S_MOD:0#\" [--read-ms 300]\n");
    std::printf("\n");
    std::printf("所有命令均可添加 --baud 19200 覆盖默认波特率；--port 缺省为自动选择。\n");
}

int RunPorts(const CliOptions& options) {
    auto discovery = lamp::CreateWindowsPortDiscovery();
    std::vector<std::string> all = discovery->GetAllPorts();
    std::vector<std::string> preferred = discovery->GetPreferredPorts();

    if (options.json) {
        std::printf("[");
        for (size_t index = 0; index < all.size(); index++) {
            if (index > 0) {
                std::printf(",");
            }
            bool is_preferred = false;
            for (const auto& port : preferred) {
                if (port == all[index]) {
                    is_preferred = true;
                }
            }
            std::printf("{\"port\":\"%s\",\"preferred\":%s}",
                        all[index].c_str(),
                        is_preferred ? "true" : "false");
        }
        std::printf("]\n");
        return 0;
    }

    if (all.empty()) {
        std::printf("未检测到串口。\n");
        return 0;
    }
    for (const auto& port : all) {
        bool is_preferred = false;
        for (const auto& candidate : preferred) {
            if (candidate == port) {
                is_preferred = true;
            }
        }
        std::printf("%s%s\n", port.c_str(),
                    is_preferred ? "  USB串口（优先）" : "");
    }
    return 0;
}

int RunStatus(const CliOptions& options) {
    lamp::Logger logger(lamp::LogLevel::Error);
    auto device = CreateDevice(&logger);
    if (!OpenDevice(device.get(), options)) {
        return 1;
    }

    lamp::DeviceStatus status;
    lamp::Result result = device->QueryStatus(status);
    if (result != lamp::Result::Ok) {
        std::fprintf(stderr, "执行失败：%s\n", device->LastError().c_str());
        return 1;
    }

    if (options.json) {
        std::printf("{\"port\":\"%s\",\"baud\":%d", device->PortName().c_str(),
                    device->BaudRate());
        std::printf(",\"version\":%d", status.version);
        std::printf(",\"mode\":%d", status.mode);
        std::printf(",\"pwm\":%d", status.pwm);
        std::printf(",\"control\":%d", status.control);
        std::printf(",\"temperature\":%d", status.temperature);
        std::printf(",\"voltage\":%d", status.voltage_raw);
        std::printf(",\"current\":%d", status.current_raw);
        std::printf(",\"errors\":%d}\n", status.error_flags);
        return 0;
    }

    std::printf("端口：%s\n", device->PortName().c_str());
    std::printf("波特率：%d\n", device->BaudRate());
    std::printf("版本：%s\n", status.version >= 0
                                  ? ("V" + std::to_string(status.version)).c_str()
                                  : "--");
    std::printf("模式：%d\n", status.mode);
    std::printf("PWM：%d\n", status.pwm);
    std::printf("输出控制：%d\n", status.control);
    std::printf("温度：%d℃\n", status.temperature);
    std::printf("电压：%.1fV\n",
                status.voltage_raw < 0 ? -1.0 : status.voltage_raw / 10.0);
    std::printf("电流：%.1fA\n",
                status.current_raw < 0 ? -1.0 : status.current_raw / 10.0);
    std::printf("保护状态：%d\n", status.error_flags);
    return 0;
}

int RunStable(const CliOptions& options) {
    lamp::Logger logger(lamp::LogLevel::Error);
    auto device = CreateDevice(&logger);
    if (!OpenDevice(device.get(), options)) {
        return 1;
    }
    device->SetMode(0);
    device->SetPwm(1);
    device->SetRestoreStableMode(true);
    lamp::Result result = device->Apply();
    if (result != lamp::Result::Ok) {
        std::fprintf(stderr, "执行失败：%s\n", device->LastError().c_str());
        return 1;
    }
    std::printf("已切换为稳定数字调光 / 95K，端口 %s。\n",
                device->PortName().c_str());
    return 0;
}

int RunSwitch(const CliOptions& options, bool turn_on) {
    lamp::Logger logger(lamp::LogLevel::Error);
    auto device = CreateDevice(&logger);
    if (!OpenDevice(device.get(), options)) {
        return 1;
    }
    for (int channel : options.lights) {
        lamp::Result result = device->SetChannel(channel,
                                                 turn_on ? options.brightness
                                                        : 0);
        if (result != lamp::Result::Ok) {
            std::fprintf(stderr, "执行失败：%s\n",
                         device->LastError().c_str());
            return 1;
        }
    }
    std::printf("%s：", turn_on ? "已开启" : "已关闭");
    for (size_t index = 0; index < options.lights.size(); index++) {
        if (index > 0) {
            std::printf("、");
        }
        std::printf("灯 %d（CH%d）", options.lights[index],
                    options.lights[index]);
    }
    if (turn_on) {
        std::printf("，亮度 %d%%。", options.brightness);
    }
    std::printf("\n");
    return 0;
}

int RunBrightness(const CliOptions& options) {
    lamp::Logger logger(lamp::LogLevel::Error);
    auto device = CreateDevice(&logger);
    if (!OpenDevice(device.get(), options)) {
        return 1;
    }
    for (int channel : options.lights) {
        lamp::Result result = options.use_raw
                                  ? device->SetChannelRaw(channel,
                                                          options.raw_value)
                                  : device->SetChannel(channel, options.value);
        if (result != lamp::Result::Ok) {
            std::fprintf(stderr, "执行失败：%s\n",
                         device->LastError().c_str());
            return 1;
        }
    }
    std::printf("亮度已设置：%s -> %d%s\n",
                options.lights.size() == static_cast<size_t>(kChannelCount)
                    ? "all"
                    : std::to_string(options.lights[0]).c_str(),
                options.use_raw ? options.raw_value : options.value,
                options.use_raw ? "" : "%");
    return 0;
}

int RunSet(const CliOptions& options) {
    lamp::Logger logger(lamp::LogLevel::Error);
    auto device = CreateDevice(&logger);
    if (!OpenDevice(device.get(), options)) {
        return 1;
    }
    std::array<int, kChannelCount> percents{};
    for (int index = 0; index < kChannelCount; index++) {
        percents[index] = options.light_values[index];
    }
    lamp::Result result = device->SetChannels(percents);
    if (result != lamp::Result::Ok) {
        std::fprintf(stderr, "执行失败：%s\n", device->LastError().c_str());
        return 1;
    }
    std::printf("CH1=%d%%，CH2=%d%%，CH3=%d%%，CH4=%d%%。\n",
                percents[0], percents[1], percents[2], percents[3]);
    return 0;
}

int RunRaw(const CliOptions& options) {
    lamp::Logger logger(lamp::LogLevel::Error);
    auto device = CreateDevice(&logger);
    if (!OpenDevice(device.get(), options)) {
        return 1;
    }
    std::string response;
    lamp::Result result =
        device->SendRaw(options.command, options.read_ms, response);
    if (result != lamp::Result::Ok) {
        std::fprintf(stderr, "执行失败：%s\n", device->LastError().c_str());
        return 1;
    }
    std::printf("TX  %s\n", options.command.c_str());
    if (!response.empty()) {
        std::printf("RX  %s\n", response.c_str());
    }
    return 0;
}

int RunCli(int argc, char** argv) {
    if (argc <= 1 || std::strcmp(argv[1], "--help") == 0 ||
        std::strcmp(argv[1], "-h") == 0) {
        PrintHelp();
        return 0;
    }

    CliOptions options;
    options.json = OptionPresent(argc, argv, "--json");
    const char* port_text = OptionValue(argc, argv, "--port");
    if (port_text != nullptr) {
        options.port = port_text;
    }
    const char* baud_text = OptionValue(argc, argv, "--baud");
    if (baud_text != nullptr) {
        if (!ParseInteger(baud_text, &options.baud) || options.baud <= 0) {
            std::fprintf(stderr, "错误：--baud 必须是正整数。\n");
            return 1;
        }
    }
    const char* light_text = OptionValue(argc, argv, "--light");
    if (light_text != nullptr &&
        !ParseLightList(light_text, &options.lights)) {
        std::fprintf(stderr, "错误：--light 只支持 1-4、all 或逗号组合。\n");
        return 1;
    }
    const char* brightness_text = OptionValue(argc, argv, "--brightness");
    if (brightness_text != nullptr) {
        if (!ParseInteger(brightness_text, &options.brightness) ||
            options.brightness < 0 || options.brightness > 100) {
            std::fprintf(stderr, "错误：--brightness 必须是 0-100 的整数。\n");
            return 1;
        }
        options.has_brightness = true;
    }
    const char* raw_text = OptionValue(argc, argv, "--raw");
    if (raw_text != nullptr) {
        if (!ParseInteger(raw_text, &options.raw_value) ||
            options.raw_value < 0 || options.raw_value > 255) {
            std::fprintf(stderr, "错误：--raw 必须是 0-255 的整数。\n");
            return 1;
        }
        options.use_raw = true;
    }
    const char* value_text = OptionValue(argc, argv, "--value");
    if (value_text != nullptr) {
        if (!ParseInteger(value_text, &options.value) || options.value < 0 ||
            options.value > 100) {
            std::fprintf(stderr, "错误：--value 必须是 0-100 的整数。\n");
            return 1;
        }
        options.has_value = true;
    }
    const char* command_text = OptionValue(argc, argv, "--command");
    if (command_text != nullptr) {
        options.command = command_text;
    }
    const char* read_text = OptionValue(argc, argv, "--read-ms");
    if (read_text != nullptr) {
        if (!ParseInteger(read_text, &options.read_ms) ||
            options.read_ms < 30 || options.read_ms > 10000) {
            std::fprintf(stderr, "错误：--read-ms 必须是 30-10000。\n");
            return 1;
        }
    }
    if (!ParseLightValues(argc, argv, &options)) {
        return 1;
    }

    const char* command = argv[1];
    if (_stricmp(command, "ports") == 0) {
        return RunPorts(options);
    }
    if (_stricmp(command, "status") == 0) {
        return RunStatus(options);
    }
    if (_stricmp(command, "stable") == 0) {
        return RunStable(options);
    }
    if (_stricmp(command, "on") == 0) {
        return RunSwitch(options, true);
    }
    if (_stricmp(command, "off") == 0) {
        return RunSwitch(options, false);
    }
    if (_stricmp(command, "brightness") == 0) {
        if (!options.use_raw && !options.has_value) {
            std::fprintf(stderr, "错误：brightness 需要 --value 或 --raw。\n");
            return 1;
        }
        return RunBrightness(options);
    }
    if (_stricmp(command, "set") == 0) {
        if (!options.has_light_values) {
            std::fprintf(stderr,
                         "错误：set 命令至少需要一个 --lightN 参数。\n");
            return 1;
        }
        return RunSet(options);
    }
    if (_stricmp(command, "raw") == 0) {
        if (options.command.empty()) {
            std::fprintf(stderr, "错误：raw 命令需要 --command。\n");
            return 1;
        }
        return RunRaw(options);
    }

    PrintHelp();
    return 2;
}

bool ShouldPauseConsole() {
    DWORD processes[4];
    DWORD count = GetConsoleProcessList(processes, 4);
    return count <= 1;
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);
    int result = RunCli(argc, argv);
    if (ShouldPauseConsole()) {
        std::printf("\n按回车键关闭窗口...");
        std::fflush(stdout);
        int character;
        do {
            character = std::getchar();
        } while (character != '\n' && character != EOF);
    }
    return result;
}
