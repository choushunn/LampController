#pragma once

#include "lamp/config/app_config.h"
#include "lamp/core/model.h"
#include "lamp/core/units.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace lamp {
namespace gui {

// 日志条目配色（与 GDI COLORREF 兼容的 RGB 位模式，不依赖 windows.h）。
namespace LogColor {
constexpr int Normal = 0xC2 | (0xC6 << 8) | (0xCE << 16);  // RGB(194,198,206)
constexpr int Tx = 0x62 | (0xA6 << 8) | (0xF4 << 16);      // RGB(98,166,244)
constexpr int Rx = 0x3F | (0xC7 << 8) | (0x84 << 16);      // RGB(63,199,132)
constexpr int Error = 0xF4 | (0x64 << 8) | (0x64 << 16);   // RGB(244,100,100)
}  // namespace LogColor

// UI 状态模型：纯数据，只允许在 UI 线程修改。
class UiModel {
public:
    static constexpr int kMaxLogEntries = 200;
    static constexpr int kDefaultBaudIndex = 1;

    ConnectionState connection_state = ConnectionState::Disconnected;
    bool connected = false;
    std::array<ChannelState, kChannelCount> channels{};
    std::vector<std::string> port_names;
    int port_index = -1;
    std::vector<std::string> baud_options{"9600", "19200", "38400", "57600",
                                          "115200"};
    int baud_index = kDefaultBaudIndex;
    std::vector<std::string> mode_options{"稳定数字调光", "频闪模式（会闪烁）"};
    int mode_index = 0;
    std::vector<std::string> pwm_options{"47.5K", "95K（推荐）"};
    int pwm_index = 1;
    bool restore_stable_mode = true;
    DeviceStatus status;
    std::vector<std::pair<std::string, int>> log_entries;
    std::string status_message = "设备状态：等待连接";

    explicit UiModel(const AppConfigData& config) {
        baud_index = -1;
        for (size_t index = 0; index < baud_options.size(); ++index) {
            if (std::stoi(baud_options[index]) == config.baud_rate) {
                baud_index = static_cast<int>(index);
                break;
            }
        }
        if (baud_index < 0) {
            baud_index = kDefaultBaudIndex;
        }
        mode_index = Clamp(config.mode, 0,
                           static_cast<int>(mode_options.size()) - 1);
        pwm_index = Clamp(config.pwm, 0,
                          static_cast<int>(pwm_options.size()) - 1);
        restore_stable_mode = config.restore_stable_mode;
        for (int index = 0; index < kChannelCount; ++index) {
            int percent =
                Clamp(config.channels[static_cast<size_t>(index)], 0, 100);
            channels[static_cast<size_t>(index)] = {
                percent > 0, percent, PercentToRaw(percent)};
        }
    }

    // 全部通道归零。
    void ResetChannels() {
        for (int index = 0; index < kChannelCount; ++index) {
            channels[static_cast<size_t>(index)] = ChannelState{};
        }
    }

    // index 为 0 基通道下标；on/percent/raw 由调用方保持一致。
    void SetChannel(int index, bool on, int percent, int raw) {
        if (index < 0 || index >= kChannelCount) {
            return;
        }
        if (percent < 0) {
            percent = 0;
        }
        if (percent > 100) {
            percent = 100;
        }
        channels[static_cast<size_t>(index)] = {on, percent, raw};
    }

    // 追加日志（带颜色），超过上限时丢弃最旧条目。
    void AddLog(const std::string& text, int color) {
        log_entries.emplace_back(text, color);
        size_t size = log_entries.size();
        if (size > static_cast<size_t>(kMaxLogEntries)) {
            log_entries.erase(
                log_entries.begin(),
                log_entries.begin() +
                    (size - static_cast<size_t>(kMaxLogEntries)));
        }
    }

    void ClearLogs() {
        log_entries.clear();
    }

private:
    static int Clamp(int value, int low, int high) {
        return value < low ? low : (value > high ? high : value);
    }
};

}  // namespace gui
}  // namespace lamp
