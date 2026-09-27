#pragma once

#include "model.h"

#include "lamp/config/app_config.h"
#include "lamp/logging/logger.h"
#include "lamp/service/lamp_device.h"

#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include <windows.h>

namespace lamp {
namespace gui {

// 工作线程 -> UI 线程的自定义消息（WM_APP + 1..5）。
constexpr UINT kMsgState = WM_APP + 1;      // wParam：ConnectionState 数值
constexpr UINT kMsgConnected = WM_APP + 2;  // wParam：是否已连接
constexpr UINT kMsgChannel = WM_APP + 3;    // 通道数据在 pending_channels_ 队列
constexpr UINT kMsgStatus = WM_APP + 4;     // 状态数据在 pending_statuses_ 队列
constexpr UINT kMsgLog = WM_APP + 5;        // 日志文本在 pending_logs_ 队列

// 滑块拖动节流定时器基号（定时器 ID = 基号 + 通道下标）。
constexpr UINT kTimerSliderBase = 1;

// 控制器：组装并持有 LampDevice，把所有 UI 操作转成设备调用，并把设备工作
// 线程的回调通过 PostMessage 桥接回 UI 线程（见 HandleMessage）。
class Controller {
public:
    Controller(UiModel& model, HWND hwnd, const AppConfigData& config);
    ~Controller();

    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;

    // 以下方法全部由 View 在 UI 线程调用。
    void OnPortRefresh();
    void OnConnect();
    // persist=false 用于滑块拖动节流（WM_TIMER），避免每 80ms 写一次配置。
    void OnSetChannel(int channel_index, int percent, bool persist = true);
    void OnToggleChannel(int channel_index);
    void OnApply();
    void OnQueryStatus();
    void OnSetPortIndex(int index);
    void OnSetBaudIndex(int index);
    void OnSetModeIndex(int index);
    void OnSetPwmIndex(int index);
    void OnSetRestoreStableMode(bool enabled);
    // 在 UI 线程直接追加一条带时间戳的日志。
    void AppendUILog(const std::string& text);

    // 处理自定义消息、滑块节流定时器与窗口关闭清理。
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);

private:
    struct PendingChannel {
        int channel = 0;
        bool on = false;
        int percent = 0;
        int raw = 0;
    };

    void PostState(ConnectionState state);
    void PostConnected(bool connected);
    void PushChannel(int channel, const ChannelState& state);
    void PushStatus(const DeviceStatus& status);
    void PushLog(const std::string& text);
    void AppendLogEntry(const std::string& text);
    int LogColorFor(const std::string& text) const;
    void SaveConfig();

    UiModel& model_;
    HWND hwnd_ = nullptr;
    AppConfigData config_;
    lamp::Logger logger_;
    std::unique_ptr<lamp::PortDiscovery> discovery_;

    // 单生产者（工作线程）单消费者（UI 线程）数据队列。
    std::mutex pending_mtx_;
    std::deque<PendingChannel> pending_channels_;
    std::deque<DeviceStatus> pending_statuses_;
    std::deque<std::string> pending_logs_;

    // 最后声明：析构时最先销毁（先 join 工作线程，保证回调不再访问本对象）。
    std::unique_ptr<lamp::LampDevice> device_;
};

}  // namespace gui
}  // namespace lamp
