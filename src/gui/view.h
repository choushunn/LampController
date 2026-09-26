#pragma once

#include "app.h"

#include <string>

#include <windows.h>

namespace lamp {
namespace gui {

class Controller;
class UiModel;

// 命中区域枚举：Power/Slider/Preset 为基数 + 通道下标（/ 预设下标）的连续区间。
enum class HitId {
    None = 0,
    Refresh,
    Connect,
    PortField,
    BaudField,
    ModeField,
    PwmField,
    Apply,
    ClearLog,
    RestoreSwitch,
    PowerBase,
    SliderBase,
    PresetBase,
};

// 视图：负责全部绘制（GDI 自绘 + 双缓冲）与鼠标输入。
class View {
public:
    View(App& app, Controller& controller, UiModel& model, HWND hwnd);

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message,
                                       WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(HWND hwnd, UINT message, WPARAM w_param,
                          LPARAM l_param);

private:
    // 布局（客户端坐标，窗口 1000x780）。
    RECT ChannelCardRect(int index) const;
    RECT PowerRect(int index) const;
    RECT SliderRect(int index) const;
    RECT PresetRect(int index, int preset) const;
    RECT PortFieldRect() const;
    RECT BaudFieldRect() const;
    RECT RefreshBtnRect() const;
    RECT ConnectBtnRect() const;
    RECT ModeFieldRect() const;
    RECT PwmFieldRect() const;
    RECT ApplyBtnRect() const;
    RECT RestoreSwitchRect() const;
    RECT ClearBtnRect() const;
    RECT LogAreaRect() const;

    HitId HitTest(int x, int y) const;
    bool PointInRect(int x, int y, const RECT& rect) const;
    bool IsSliderHit(HitId hit) const;
    int SliderIndexFromHit(HitId hit) const;
    bool IsPowerHit(HitId hit) const;
    int PowerIndexFromHit(HitId hit) const;
    bool IsPresetHit(HitId hit) const;
    int SliderPercentFromX(int index, int x) const;
    int VisibleLogLines() const;
    int LogScrollMaximum() const;
    void HandleClick(HitId hit);
    void OpenChoiceMenu(HitId hit);
    void SetSliderValue(int index, int percent);

    // 绘制。
    void Paint(HDC target);
    void DrawText(HDC dc, const std::wstring& text, const RECT& rect,
                  FontId font, COLORREF color, UINT format) const;
    void FillRoundRect(HDC dc, const RECT& rect, int radius, COLORREF fill,
                       COLORREF border) const;
    void DrawCard(HDC dc, const RECT& rect, const std::wstring& title) const;
    void DrawField(HDC dc, const RECT& rect, const std::wstring& text,
                   bool hovered) const;
    void DrawButton(HDC dc, const RECT& rect, const std::wstring& text,
                    bool primary, bool hovered, bool enabled) const;
    void DrawSwitch(HDC dc, const RECT& rect, bool on) const;
    void DrawStatusPill(HDC dc) const;
    void DrawChannelCard(HDC dc, int index) const;
    void DrawDeviceCard(HDC dc) const;
    void DrawModeCard(HDC dc) const;
    void DrawLogCard(HDC dc) const;
    void DrawStatusBar(HDC dc) const;
    void DrawBulb(HDC dc, const RECT& card, bool on) const;
    void DrawPowerButton(HDC dc, const RECT& rect, bool on) const;
    void DrawSlider(HDC dc, const RECT& track, int percent) const;
    void DrawPresets(HDC dc, int index) const;

    std::wstring Utf8ToWide(const std::string& text) const;
    std::wstring TelemetryLine1() const;
    std::wstring TelemetryLine2() const;
    std::wstring ConnectButtonText() const;
    bool ConnectButtonEnabled() const;
    std::wstring FormatVoltage(int raw_value) const;
    std::wstring FormatCurrent(int raw_value) const;

    App& app_;
    Controller& controller_;
    UiModel& model_;
    HWND hwnd_ = nullptr;
    int hover_hit_ = static_cast<int>(HitId::None);
    int mouse_x_ = 0;
    int mouse_y_ = 0;
    int dragging_slider_ = -1;  // 正在拖动的通道下标，-1 表示无。
    mutable int log_scroll_ = 0;  // 日志滚动位置（绘制缓存，可在 const 绘制中修正）。
    size_t last_log_count_ = 0;
};

}  // namespace gui
}  // namespace lamp
