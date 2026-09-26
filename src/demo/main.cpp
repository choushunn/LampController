#include "lamp/sdk/lamp_sdk.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

namespace {

void PrintHelp() {
    std::printf("LampController C SDK demo %s\n", lamp_sdk_version());
    std::printf("\n");
    std::printf("  LampSdkDemo.exe ports\n");
    std::printf("  LampSdkDemo.exe selftest\n");
    std::printf("  LampSdkDemo.exe status COM9\n");
    std::printf("  LampSdkDemo.exe set COM9 60 30 40 50\n");
    std::printf("  LampSdkDemo.exe off COM9\n");
    std::printf("\n");
    std::printf("The set command switches all four channels to stable 95K mode.\n");
}

bool ParsePercent(const char* text, const char* name, int* value) {
    char* end = nullptr;
    long parsed = std::strtol(text, &end, 10);
    if (end == text || *end != '\0' || parsed < 0 || parsed > 100) {
        std::fprintf(stderr, "%s must be an integer from 0 to 100.\n", name);
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

int PrintPorts() {
    char ports[512] = {};
    char usb_ports[512] = {};
    int count = lamp_sdk_get_port_names(ports, sizeof(ports));
    if (count <= 0) {
        std::printf("No serial port detected.\n");
        return 0;
    }
    lamp_sdk_get_usb_port_names(usb_ports, sizeof(usb_ports));
    std::string usb(usb_ports);
    char* cursor = ports;
    while (cursor != nullptr && *cursor != '\0') {
        char* separator = std::strchr(cursor, ';');
        if (separator != nullptr) {
            *separator = '\0';
        }
        std::string port(cursor);
        std::printf("%s%s\n", port.c_str(),
                    usb.find(port) != std::string::npos ? "  USB" : "");
        cursor = separator != nullptr ? separator + 1 : nullptr;
    }
    return 0;
}

int PrintStatus(const char* port) {
    lamp_sdk_t* sdk = lamp_sdk_create(port, LAMP_SDK_DEFAULT_BAUD_RATE);
    if (sdk == nullptr) {
        std::fprintf(stderr, "Error: %s\n", lamp_sdk_get_last_error(nullptr));
        return 1;
    }
    int result = lamp_sdk_connect(sdk);
    lamp_sdk_status_t status = {};
    if (result == LAMP_SDK_OK) {
        result = lamp_sdk_query_status(sdk, &status);
    }
    if (result != LAMP_SDK_OK) {
        std::fprintf(stderr, "Error: %s\n", lamp_sdk_get_last_error(sdk));
        lamp_sdk_destroy(sdk);
        return 1;
    }

    std::printf("Port: %s\n", status.port_name);
    std::printf("Baud: %d\n", status.baud_rate);
    std::printf("Version: %d\n", status.version);
    std::printf("Mode: %d\n", status.mode);
    std::printf("PWM: %d\n", status.pwm);
    std::printf("Control: %d\n", status.control);
    std::printf("Temperature: %d C\n", status.temperature);
    std::printf("Voltage: %.1f V\n",
                status.voltage_raw < 0 ? -1.0 : status.voltage_raw / 10.0);
    std::printf("Current: %.1f A\n",
                status.current_raw < 0 ? -1.0 : status.current_raw / 10.0);
    std::printf("Errors: %d\n", status.error_flags);
    lamp_sdk_destroy(sdk);
    return 0;
}

int SetChannels(const char* port, const std::vector<int>& percents) {
    lamp_sdk_t* sdk = lamp_sdk_create(port, LAMP_SDK_DEFAULT_BAUD_RATE);
    if (sdk == nullptr) {
        std::fprintf(stderr, "Error: %s\n", lamp_sdk_get_last_error(nullptr));
        return 1;
    }
    int result = lamp_sdk_connect(sdk);
    if (result == LAMP_SDK_OK) {
        result = lamp_sdk_set_restore_stable_mode(sdk, 1);
    }
    if (result == LAMP_SDK_OK) {
        int values[LAMP_SDK_CHANNEL_COUNT] = {0, 0, 0, 0};
        for (size_t index = 0; index < percents.size() &&
                              index < LAMP_SDK_CHANNEL_COUNT;
             index++) {
            values[index] = percents[index];
        }
        result = lamp_sdk_set_brightness_percent_all(sdk, values);
    }
    if (result != LAMP_SDK_OK) {
        std::fprintf(stderr, "Error: %s\n", lamp_sdk_get_last_error(sdk));
        lamp_sdk_destroy(sdk);
        return 1;
    }

    std::printf("port=%s, channels:", port);
    for (size_t index = 0; index < percents.size() &&
                              index < LAMP_SDK_CHANNEL_COUNT;
         index++) {
        std::printf(" CH%d=%d%%", static_cast<int>(index + 1), percents[index]);
    }
    std::printf("\n");
    lamp_sdk_destroy(sdk);
    return 0;
}

int TurnOffAll(const char* port) {
    lamp_sdk_t* sdk = lamp_sdk_create(port, LAMP_SDK_DEFAULT_BAUD_RATE);
    if (sdk == nullptr) {
        std::fprintf(stderr, "Error: %s\n", lamp_sdk_get_last_error(nullptr));
        return 1;
    }
    int result = lamp_sdk_connect(sdk);
    if (result == LAMP_SDK_OK) {
        result = lamp_sdk_turn_off_all(sdk);
    }
    if (result != LAMP_SDK_OK) {
        std::fprintf(stderr, "Error: %s\n", lamp_sdk_get_last_error(sdk));
        lamp_sdk_destroy(sdk);
        return 1;
    }
    std::printf("All channels are off, port=%s\n", port);
    lamp_sdk_destroy(sdk);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    SetConsoleOutputCP(CP_UTF8);

    if (argc <= 1 || std::strcmp(argv[1], "--help") == 0 ||
        std::strcmp(argv[1], "-h") == 0) {
        PrintHelp();
        return 0;
    }

    if (_stricmp(argv[1], "ports") == 0) {
        return PrintPorts();
    }

    if (_stricmp(argv[1], "selftest") == 0) {
        char command[32] = {};
        if (lamp_sdk_percent_to_raw(0) != 0 ||
            lamp_sdk_percent_to_raw(50) != 128 ||
            lamp_sdk_percent_to_raw(100) != 255 ||
            lamp_sdk_build_channel_command(1, 153, command, sizeof(command)) !=
                LAMP_SDK_OK ||
            std::strcmp(command, "SPA153#") != 0) {
            std::fprintf(stderr, "SDK self-test failed.\n");
            return 1;
        }
        std::printf("SDK self-test passed.\n");
        return 0;
    }

    if (_stricmp(argv[1], "status") == 0 && argc >= 3) {
        return PrintStatus(argv[2]);
    }

    if (_stricmp(argv[1], "set") == 0 && argc >= 4) {
        std::vector<int> percents;
        for (int index = 3; index < argc && index < 3 + LAMP_SDK_CHANNEL_COUNT;
             index++) {
            int value = 0;
            std::string name = "CH" + std::to_string(index - 2) + " brightness";
            if (!ParsePercent(argv[index], name.c_str(), &value)) {
                return 2;
            }
            percents.push_back(value);
        }
        return SetChannels(argv[2], percents);
    }

    if (_stricmp(argv[1], "off") == 0 && argc >= 3) {
        return TurnOffAll(argv[2]);
    }

    PrintHelp();
    return 2;
}
