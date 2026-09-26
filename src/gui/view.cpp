#include "view.h"

#include "app.h"
#include "controller.h"
#include "model.h"

#include "lamp/core/units.h"

#include <cmath>
#include <cwchar>
#include <string>
#include <vector>

#include <windowsx.h>

namespace lamp {
namespace gui {

namespace {

// 深色视觉风格（继承自旧版 LampController.c）。
constexpr COLORREF kBackground = RGB(13, 15, 20);
constexpr COLORREF kCard = RGB(23, 26, 32);
constexpr COLORREF kCardBorder = RGB(42, 47, 58);
constexpr COLORREF kField = RGB(30, 34, 42);
constexpr COLORREF kFieldHover = RGB(37, 42, 52);
constexpr COLORREF kText = RGB(244, 246, 250);
constexpr COLORREF kMuted = RGB(143, 150, 164);
constexpr COLORREF kAccent = RGB(246, 174, 69);
constexpr COLORREF kAccentHover = RGB(255, 193, 102);
constexpr COLORREF kAccentDark = RGB(107, 72, 19);
constexpr COLORREF kGreen = RGB(63, 199, 132);
constexpr COLORREF kRed = RGB(244, 100, 100);
constexpr COLORREF kBorderLight = RGB(235, 237, 241);

constexpr int kPresetCount = 5;
constexpr int kPresetValues[kPresetCount] = {10, 30, 50, 80, 100};

// 右侧三张卡片与底部状态栏（通道卡在左侧 2x2 网格，动态计算）。
constexpr RECT kDeviceCard = {620, 112, 974, 326};
constexpr RECT kModeCard = {620, 340, 974, 540};
constexpr RECT kLogCard = {620, 554, 974, 718};
constexpr RECT kStatusBar = {26, 730, 970, 764};

int HitValue(HitId id) {
    return static_cast<int>(id);
}

}  // namespace

View::View(App& app, Controller& controller, UiModel& model, HWND hwnd)
    : app_(app), controller_(controller), model_(model), hwnd_(hwnd) {}

LRESULT CALLBACK View::WindowProc(HWND hwnd, UINT message, WPARAM w_param,
                                  LPARAM l_param) {
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
        App* app = static_cast<App*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(app));
        return DefWindowProcW(hwnd, message, w_param, l_param);
    }
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app != nullptr) {
        View* view = app->GetView();
        if (view != nullptr) {
            return view->HandleMessage(hwnd, message, w_param, l_param);
        }
        if (message == WM_CREATE) {
            app->OnCreate(hwnd);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

LRESULT View::HandleMessage(HWND hwnd, UINT message, WPARAM w_param,
                            LPARAM l_param) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        Paint(dc);
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_MOUSEMOVE: {
        mouse_x_ = GET_X_LPARAM(l_param);
        mouse_y_ = GET_Y_LPARAM(l_param);
        if (dragging_slider_ >= 0) {
            SetSliderValue(dragging_slider_,
                           SliderPercentFromX(dragging_slider_, mouse_x_));
        }
        int hit = HitValue(HitTest(mouse_x_, mouse_y_));
        if (hit != hover_hit_) {
            hover_hit_ = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(l_param);
        int y = GET_Y_LPARAM(l_param);
        HitId hit = HitTest(x, y);
        if (IsSliderHit(hit)) {
            int index = SliderIndexFromHit(hit);
            dragging_slider_ = index;
            SetCapture(hwnd);
            SetSliderValue(index, SliderPercentFromX(index, x));
            // 拖动节流：每 80ms 由 WM_TIMER 向控制器发送一次当前亮度。
            SetTimer(hwnd, kTimerSliderBase + static_cast<UINT_PTR>(index), 80,
                     nullptr);
        } else {
            HandleClick(hit);
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (dragging_slider_ >= 0) {
            int index = dragging_slider_;
            dragging_slider_ = -1;
            KillTimer(hwnd, kTimerSliderBase + static_cast<UINT_PTR>(index));
            ReleaseCapture();
            // 松手立即发送最终亮度。
            controller_.OnSetChannel(
                index, model_.channels[static_cast<size_t>(index)].percent);
        }
        return 0;

    case WM_LBUTTONDBLCLK: {
        int x = GET_X_LPARAM(l_param);
        int y = GET_Y_LPARAM(l_param);
        if (PointInRect(x, y, kDeviceCard)) {
            controller_.OnQueryStatus();
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        POINT point = {GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        ScreenToClient(hwnd, &point);
        if (PointInRect(point.x, point.y, LogAreaRect())) {
            int delta = GET_WHEEL_DELTA_WPARAM(w_param);
            int maximum = LogScrollMaximum();
            log_scroll_ += delta / WHEEL_DELTA;
            if (log_scroll_ < 0) {
                log_scroll_ = 0;
            }
            if (log_scroll_ > maximum) {
                log_scroll_ = maximum;
            }
            InvalidateRect(hwnd, &kLogCard, FALSE);
        }
        return 0;
    }

    case WM_TIMER:
        return controller_.HandleMessage(message, w_param, l_param);

    case kMsgState:
    case kMsgConnected:
    case kMsgChannel:
    case kMsgStatus:
    case kMsgLog:
        controller_.HandleMessage(message, w_param, l_param);
        // 新增日志时自动滚动到底部。
        if (model_.log_entries.size() != last_log_count_) {
            last_log_count_ = model_.log_entries.size();
            log_scroll_ = 0;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        controller_.HandleMessage(message, w_param, l_param);
        app_.OnDestroy();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

// ---------------------------------------------------------------------------
// 布局
// ---------------------------------------------------------------------------

RECT View::ChannelCardRect(int index) const {
    int column = index % 2;
    int row = index / 2;
    int left = 26 + column * 270;
    int top = 112 + row * 314;
    return {left, top, left + 260, top + 300};
}

RECT View::PowerRect(int index) const {
    RECT card = ChannelCardRect(index);
    return {card.left + 16, card.top + 170, card.left + 65, card.top + 219};
}

RECT View::SliderRect(int index) const {
    RECT card = ChannelCardRect(index);
    return {card.left + 16, card.top + 248, card.right - 16, card.top + 269};
}

RECT View::PresetRect(int index, int preset) const {
    RECT card = ChannelCardRect(index);
    int left = card.left + 16 + preset * 46;
    return {left, card.top + 274, left + 40, card.top + 298};
}

RECT View::PortFieldRect() const {
    return {690, kDeviceCard.top + 50, 960, kDeviceCard.top + 82};
}

RECT View::BaudFieldRect() const {
    return {690, kDeviceCard.top + 86, 960, kDeviceCard.top + 118};
}

RECT View::RefreshBtnRect() const {
    return {680, kDeviceCard.top + 124, 780, kDeviceCard.top + 158};
}

RECT View::ConnectBtnRect() const {
    return {786, kDeviceCard.top + 124, 960, kDeviceCard.top + 158};
}

RECT View::ModeFieldRect() const {
    return {690, kModeCard.top + 46, 960, kModeCard.top + 78};
}

RECT View::PwmFieldRect() const {
    return {690, kModeCard.top + 80, 960, kModeCard.top + 112};
}

RECT View::ApplyBtnRect() const {
    return {680, kModeCard.top + 118, 960, kModeCard.top + 152};
}

RECT View::RestoreSwitchRect() const {
    return {860, kModeCard.top + 162, 930, kModeCard.top + 192};
}

RECT View::ClearBtnRect() const {
    return {886, kLogCard.top + 14, 956, kLogCard.top + 42};
}

RECT View::LogAreaRect() const {
    return {640, kLogCard.top + 46, 960, kLogCard.top + 162};
}

// ---------------------------------------------------------------------------
// 输入
// ---------------------------------------------------------------------------

HitId View::HitTest(int x, int y) const {
    if (PointInRect(x, y, RefreshBtnRect())) {
        return HitId::Refresh;
    }
    if (PointInRect(x, y, ConnectBtnRect())) {
        return HitId::Connect;
    }
    if (PointInRect(x, y, PortFieldRect())) {
        return HitId::PortField;
    }
    if (PointInRect(x, y, BaudFieldRect())) {
        return HitId::BaudField;
    }
    if (PointInRect(x, y, ModeFieldRect())) {
        return HitId::ModeField;
    }
    if (PointInRect(x, y, PwmFieldRect())) {
        return HitId::PwmField;
    }
    if (PointInRect(x, y, ApplyBtnRect())) {
        return HitId::Apply;
    }
    if (PointInRect(x, y, ClearBtnRect())) {
        return HitId::ClearLog;
    }
    if (PointInRect(x, y, RestoreSwitchRect())) {
        return HitId::RestoreSwitch;
    }
    for (int index = 0; index < kChannelCount; ++index) {
        if (PointInRect(x, y, PowerRect(index))) {
            return static_cast<HitId>(HitValue(HitId::PowerBase) + index);
        }
        if (PointInRect(x, y, SliderRect(index))) {
            return static_cast<HitId>(HitValue(HitId::SliderBase) + index);
        }
        for (int preset = 0; preset < kPresetCount; ++preset) {
            if (PointInRect(x, y, PresetRect(index, preset))) {
                return static_cast<HitId>(HitValue(HitId::PresetBase) +
                                          index * kPresetCount + preset);
            }
        }
    }
    return HitId::None;
}

bool View::PointInRect(int x, int y, const RECT& rect) const {
    return x >= rect.left && x < rect.right && y >= rect.top &&
           y < rect.bottom;
}

bool View::IsSliderHit(HitId hit) const {
    int value = HitValue(hit);
    return value >= HitValue(HitId::SliderBase) &&
           value < HitValue(HitId::SliderBase) + kChannelCount;
}

int View::SliderIndexFromHit(HitId hit) const {
    return HitValue(hit) - HitValue(HitId::SliderBase);
}

bool View::IsPowerHit(HitId hit) const {
    int value = HitValue(hit);
    return value >= HitValue(HitId::PowerBase) &&
           value < HitValue(HitId::PowerBase) + kChannelCount;
}

int View::PowerIndexFromHit(HitId hit) const {
    return HitValue(hit) - HitValue(HitId::PowerBase);
}

bool View::IsPresetHit(HitId hit) const {
    int value = HitValue(hit);
    return value >= HitValue(HitId::PresetBase) &&
           value < HitValue(HitId::PresetBase) + kChannelCount * kPresetCount;
}

int View::SliderPercentFromX(int index, int x) const {
    RECT track = SliderRect(index);
    if (x <= track.left) {
        return 0;
    }
    if (x >= track.right) {
        return 100;
    }
    int percent = (x - track.left) * 100 / (track.right - track.left);
    return percent < 0 ? 0 : (percent > 100 ? 100 : percent);
}

int View::VisibleLogLines() const {
    RECT area = LogAreaRect();
    int visible = (area.bottom - area.top - 12) / 17;
    return visible < 1 ? 1 : visible;
}

int View::LogScrollMaximum() const {
    int count = static_cast<int>(model_.log_entries.size());
    int maximum = count - VisibleLogLines();
    return maximum < 0 ? 0 : maximum;
}

void View::SetSliderValue(int index, int percent) {
    if (index < 0 || index >= kChannelCount) {
        return;
    }
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    model_.SetChannel(index, percent > 0, percent, PercentToRaw(percent));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void View::HandleClick(HitId hit) {
    switch (hit) {
    case HitId::Refresh:
        controller_.OnPortRefresh();
        controller_.AppendUILog("已刷新串口列表。");
        break;
    case HitId::Connect:
        controller_.OnConnect();
        break;
    case HitId::PortField:
    case HitId::BaudField:
    case HitId::ModeField:
    case HitId::PwmField:
        OpenChoiceMenu(hit);
        break;
    case HitId::Apply:
        controller_.OnApply();
        break;
    case HitId::ClearLog:
        model_.ClearLogs();
        log_scroll_ = 0;
        last_log_count_ = 0;
        InvalidateRect(hwnd_, nullptr, FALSE);
        break;
    case HitId::RestoreSwitch:
        controller_.OnSetRestoreStableMode(!model_.restore_stable_mode);
        break;
    default:
        if (IsPowerHit(hit)) {
            controller_.OnToggleChannel(PowerIndexFromHit(hit));
        } else if (IsPresetHit(hit)) {
            int value = HitValue(hit) - HitValue(HitId::PresetBase);
            int index = value / kPresetCount;
            int preset = value % kPresetCount;
            controller_.OnSetChannel(index, kPresetValues[preset]);
        }
        break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void View::OpenChoiceMenu(HitId hit) {
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    RECT field_rect = {};
    int current = -1;
    std::vector<std::wstring> items;

    if (hit == HitId::PortField) {
        for (const auto& port : model_.port_names) {
            items.push_back(Utf8ToWide(port));
        }
        current = model_.port_index;
        field_rect = PortFieldRect();
    } else if (hit == HitId::BaudField) {
        for (const auto& option : model_.baud_options) {
            items.push_back(Utf8ToWide(option));
        }
        current = model_.baud_index;
        field_rect = BaudFieldRect();
    } else if (hit == HitId::ModeField) {
        for (const auto& option : model_.mode_options) {
            items.push_back(Utf8ToWide(option));
        }
        current = model_.mode_index;
        field_rect = ModeFieldRect();
    } else {
        for (const auto& option : model_.pwm_options) {
            items.push_back(Utf8ToWide(option));
        }
        current = model_.pwm_index;
        field_rect = PwmFieldRect();
    }

    int count = static_cast<int>(items.size());
    if (count <= 0) {
        DestroyMenu(menu);
        controller_.AppendUILog("当前没有可选串口，请连接设备后刷新。");
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    for (int index = 0; index < count; ++index) {
        AppendMenuW(menu, MF_STRING, static_cast<UINT_PTR>(index + 1),
                    items[static_cast<size_t>(index)].c_str());
    }
    if (current >= 0 && current < count) {
        CheckMenuRadioItem(menu, 1, static_cast<UINT>(count),
                           static_cast<UINT>(current + 1), MF_BYCOMMAND);
    }
    POINT point = {field_rect.left, field_rect.bottom};
    ClientToScreen(hwnd_, &point);
    int selected = static_cast<int>(TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, point.x, point.y,
        0, hwnd_, nullptr));
    DestroyMenu(menu);
    if (selected <= 0) {
        return;
    }
    selected--;
    switch (hit) {
    case HitId::PortField:
        controller_.OnSetPortIndex(selected);
        break;
    case HitId::BaudField:
        controller_.OnSetBaudIndex(selected);
        break;
    case HitId::ModeField:
        controller_.OnSetModeIndex(selected);
        break;
    default:
        controller_.OnSetPwmIndex(selected);
        break;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void View::Paint(HDC target) {
    RECT client;
    GetClientRect(hwnd_, &client);
    HDC memory_dc = CreateCompatibleDC(target);
    HBITMAP bitmap = CreateCompatibleBitmap(target, client.right - client.left,
                                            client.bottom - client.top);
    HGDIOBJ old_bitmap = SelectObject(memory_dc, bitmap);

    HBRUSH background = CreateSolidBrush(kBackground);
    FillRect(memory_dc, &client, background);
    DeleteObject(background);

    DrawText(memory_dc, L"智能灯光控制器", {26, 24, 500, 66}, FontId::Title,
             kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawText(memory_dc, L"四灯独立开关、亮度与稳定调光控制", {28, 68, 520, 92},
             FontId::Subtitle, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawStatusPill(memory_dc);

    for (int index = 0; index < kChannelCount; ++index) {
        DrawChannelCard(memory_dc, index);
    }
    DrawDeviceCard(memory_dc);
    DrawModeCard(memory_dc);
    DrawLogCard(memory_dc);
    DrawStatusBar(memory_dc);

    BitBlt(target, 0, 0, client.right, client.bottom, memory_dc, 0, 0,
           SRCCOPY);
    SelectObject(memory_dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(memory_dc);
}

void View::DrawText(HDC dc, const std::wstring& text, const RECT& rect,
                    FontId font, COLORREF color, UINT format) const {
    if (text.empty()) {
        return;
    }
    HGDIOBJ old_font = SelectObject(dc, app_.Font(font));
    int old_mode = SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    RECT adjusted = rect;
    DrawTextW(dc, text.c_str(), -1, &adjusted, format);
    SetBkMode(dc, old_mode);
    SelectObject(dc, old_font);
}

void View::FillRoundRect(HDC dc, const RECT& rect, int radius, COLORREF fill,
                         COLORREF border) const {
    HBRUSH brush = CreateSolidBrush(fill);
    HPEN created_pen =
        (border == CLR_INVALID) ? nullptr : CreatePen(PS_SOLID, 1, border);
    HPEN pen = created_pen != nullptr
                   ? created_pen
                   : static_cast<HPEN>(GetStockObject(NULL_PEN));
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius,
              radius);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    if (created_pen != nullptr) {
        DeleteObject(created_pen);
    }
    DeleteObject(brush);
}

void View::DrawCard(HDC dc, const RECT& rect,
                    const std::wstring& title) const {
    FillRoundRect(dc, rect, 12, kCard, kCardBorder);
    if (!title.empty()) {
        DrawText(dc, title,
                 {rect.left + 20, rect.top + 12, rect.right - 20,
                  rect.top + 44},
                 FontId::CardTitle, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

void View::DrawField(HDC dc, const RECT& rect, const std::wstring& text,
                     bool hovered) const {
    FillRoundRect(dc, rect, 4, hovered ? kFieldHover : kField,
                  hovered ? kAccent : kBorderLight);
    DrawText(dc, text,
             {rect.left + 12, rect.top + 2, rect.right - 30, rect.bottom - 2},
             FontId::Value, kText,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    POINT arrow[3] = {
        {rect.right - 20, rect.top + 12},
        {rect.right - 11, rect.top + 12},
        {rect.right - 15, rect.top + 19},
    };
    HGDIOBJ old_brush = SelectObject(dc, CreateSolidBrush(kText));
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Polygon(dc, arrow, 3);
    SelectObject(dc, old_pen);
    DeleteObject(SelectObject(dc, old_brush));
}

void View::DrawButton(HDC dc, const RECT& rect, const std::wstring& text,
                      bool primary, bool hovered, bool enabled) const {
    COLORREF fill = primary ? kAccent : kField;
    COLORREF text_color = primary ? RGB(34, 27, 15) : kText;
    COLORREF border = primary ? CLR_INVALID : kCardBorder;
    if (hovered && enabled) {
        fill = primary ? kAccentHover : kFieldHover;
    }
    if (!enabled) {
        fill = RGB(24, 27, 33);
        text_color = kMuted;
        border = kCardBorder;
    }
    FillRoundRect(dc, rect, 6, fill, border);
    DrawText(dc, text, rect, FontId::Button, text_color,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void View::DrawSwitch(HDC dc, const RECT& rect, bool on) const {
    FillRoundRect(dc, rect, 15, on ? kAccent : RGB(49, 54, 64),
                  on ? kAccent : kCardBorder);
    int thumb_x = on ? rect.right - 18 : rect.left + 8;
    HBRUSH thumb_brush =
        CreateSolidBrush(on ? RGB(249, 247, 243) : RGB(146, 153, 166));
    HGDIOBJ old_brush = SelectObject(dc, thumb_brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, thumb_x, rect.top + 6, thumb_x + 18, rect.bottom - 6);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(thumb_brush);
}

void View::DrawStatusPill(HDC dc) const {
    RECT pill = {765, 31, 972, 69};
    FillRoundRect(dc, pill, 20, kCard, kCardBorder);
    COLORREF dot = kMuted;
    std::wstring text = L"未连接";
    switch (model_.connection_state) {
    case ConnectionState::Connected:
        dot = kGreen;
        text = L"已连接";
        break;
    case ConnectionState::Connecting:
        dot = kAccent;
        text = L"连接中...";
        break;
    case ConnectionState::Reconnecting:
        dot = kAccent;
        text = L"正在重连...";
        break;
    case ConnectionState::Failed:
        dot = kRed;
        text = L"连接失败";
        break;
    case ConnectionState::Disconnected:
        break;
    }
    HBRUSH dot_brush = CreateSolidBrush(dot);
    HGDIOBJ old_brush = SelectObject(dc, dot_brush);
    HGDIOBJ old_pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, 784, 44, 796, 56);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(dot_brush);
    DrawText(dc, text, {804, 37, pill.right - 12, 63}, FontId::Label, kText,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void View::DrawChannelCard(HDC dc, int index) const {
    RECT card = ChannelCardRect(index);
    const ChannelState& channel = model_.channels[static_cast<size_t>(index)];

    DrawCard(dc, card, L"");
    DrawText(dc, L"灯 " + std::to_wstring(index + 1),
             {card.left + 20, card.top + 14, card.right - 90, card.top + 44},
             FontId::CardTitle, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    std::wstring badge = L"CH" + std::to_wstring(index + 1);
    FillRoundRect(dc, {card.right - 74, card.top + 15, card.right - 20,
                       card.top + 44},
                  5, kAccentDark, kAccent);
    DrawText(dc, badge,
             {card.right - 74, card.top + 15, card.right - 20, card.top + 44},
             FontId::Button, kAccent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawBulb(dc, card, channel.on);
    DrawText(dc, channel.on ? L"已开启" : L"已关闭",
             {card.left + 20, card.top + 140, card.right - 20, card.top + 166},
             FontId::CardTitle, channel.on ? kAccent : kMuted,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawPowerButton(dc, PowerRect(index), channel.on);

    std::wstring percent_text = std::to_wstring(channel.percent) + L"%";
    DrawText(dc, percent_text,
             {card.left + 80, card.top + 168, card.right - 16, card.top + 220},
             FontId::Percent, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    std::wstring output_text = L"CH" + std::to_wstring(index + 1) +
                               L" · 输出 " + std::to_wstring(channel.raw) +
                               L" / 255";
    DrawText(dc, output_text,
             {card.left + 80, card.top + 220, card.right - 16, card.top + 242},
             FontId::Small, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    DrawSlider(dc, SliderRect(index), channel.percent);
    DrawPresets(dc, index);
}

void View::DrawDeviceCard(HDC dc) const {
    RECT card = kDeviceCard;
    DrawCard(dc, card, L"设备连接");

    DrawText(dc, L"端口", {640, card.top + 52, 686, card.top + 78},
             FontId::Label, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    std::wstring port_text;
    if (model_.port_index >= 0 &&
        static_cast<size_t>(model_.port_index) < model_.port_names.size()) {
        port_text =
            Utf8ToWide(model_.port_names[static_cast<size_t>(model_.port_index)]);
    } else {
        port_text = model_.port_names.empty() ? L"未检测到串口"
                                              : L"自动选择（推荐）";
    }
    DrawField(dc, PortFieldRect(), port_text,
              hover_hit_ == HitValue(HitId::PortField));

    DrawText(dc, L"波特率", {640, card.top + 88, 686, card.top + 114},
             FontId::Label, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    std::wstring baud_text =
        Utf8ToWide(model_.baud_options[static_cast<size_t>(model_.baud_index)]);
    DrawField(dc, BaudFieldRect(), baud_text,
              hover_hit_ == HitValue(HitId::BaudField));

    DrawButton(dc, RefreshBtnRect(), L"刷新", false,
               hover_hit_ == HitValue(HitId::Refresh), true);
    DrawButton(dc, ConnectBtnRect(), ConnectButtonText(), true,
               hover_hit_ == HitValue(HitId::Connect),
               ConnectButtonEnabled());

    DrawText(dc, TelemetryLine1(), {640, card.top + 164, 960, card.top + 186},
             FontId::Small, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawText(dc, TelemetryLine2(), {640, card.top + 186, 960, card.top + 208},
             FontId::Small, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void View::DrawModeCard(HDC dc) const {
    RECT card = kModeCard;
    DrawCard(dc, card, L"调光参数");

    DrawText(dc, L"模式", {640, card.top + 48, 686, card.top + 74},
             FontId::Label, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    std::wstring mode_text =
        Utf8ToWide(model_.mode_options[static_cast<size_t>(model_.mode_index)]);
    DrawField(dc, ModeFieldRect(), mode_text,
              hover_hit_ == HitValue(HitId::ModeField));

    DrawText(dc, L"频率", {640, card.top + 82, 686, card.top + 108},
             FontId::Label, kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    std::wstring pwm_text =
        Utf8ToWide(model_.pwm_options[static_cast<size_t>(model_.pwm_index)]);
    DrawField(dc, PwmFieldRect(), pwm_text,
              hover_hit_ == HitValue(HitId::PwmField));

    DrawButton(dc, ApplyBtnRect(), L"应用设置", true,
               hover_hit_ == HitValue(HitId::Apply), true);

    DrawText(dc, L"恢复稳定模式", {640, card.top + 160, 770, card.top + 186},
             FontId::Label, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DrawSwitch(dc, RestoreSwitchRect(), model_.restore_stable_mode);
}

void View::DrawLogCard(HDC dc) const {
    RECT card = kLogCard;
    DrawCard(dc, card, L"通信记录");
    DrawButton(dc, ClearBtnRect(), L"清空", false,
               hover_hit_ == HitValue(HitId::ClearLog), true);

    RECT area = LogAreaRect();
    FillRoundRect(dc, area, 5, kField, kCardBorder);

    const int line_height = 17;
    const int visible = VisibleLogLines();
    if (model_.log_entries.empty()) {
        DrawText(dc, L"暂无通信记录",
                 {area.left + 12, area.top + 8, area.right - 12,
                  area.bottom - 8},
                 FontId::Log, kMuted, DT_LEFT | DT_TOP | DT_SINGLELINE);
        return;
    }

    int maximum = LogScrollMaximum();
    if (log_scroll_ > maximum) {
        log_scroll_ = maximum;
    }
    int first = maximum - log_scroll_;
    int count = static_cast<int>(model_.log_entries.size());
    int y = area.top + 6;
    for (int line = first; line < count && line < first + visible; ++line) {
        const auto& entry = model_.log_entries[static_cast<size_t>(line)];
        DrawText(dc, Utf8ToWide(entry.first),
                 {area.left + 10, y, area.right - 16, y + line_height},
                 FontId::Log, static_cast<COLORREF>(entry.second),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += line_height;
    }

    if (count > visible) {
        int track_top = area.top + 6;
        int track_bottom = area.bottom - 6;
        const int thumb_height = 34;
        int travel = track_bottom - track_top - thumb_height;
        int thumb_top = track_top;
        if (maximum > 0) {
            thumb_top += travel * (maximum - log_scroll_) / maximum;
        }
        FillRoundRect(dc, {area.right - 7, track_top, area.right - 3,
                           track_bottom},
                      2, RGB(49, 54, 64), CLR_INVALID);
        FillRoundRect(dc, {area.right - 8, thumb_top, area.right - 2,
                           thumb_top + thumb_height},
                      3, RGB(146, 153, 166), CLR_INVALID);
    }
}

void View::DrawStatusBar(HDC dc) const {
    DrawText(dc, Utf8ToWide(model_.status_message), kStatusBar, FontId::Small,
             kMuted, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    std::wstring channels_text;
    for (int index = 0; index < kChannelCount; ++index) {
        if (index > 0) {
            channels_text += L"   ";
        }
        channels_text += L"CH" + std::to_wstring(index + 1);
        channels_text += model_.connected ? L" 已接入" : L" 未连接";
    }
    DrawText(dc, channels_text, {600, kStatusBar.top, 970, kStatusBar.bottom},
             FontId::Small, model_.connected ? kGreen : kMuted,
             DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

void View::DrawBulb(HDC dc, const RECT& card, bool on) const {
    int center_x = (card.left + card.right) / 2;
    int bulb_top = card.top + 52;

    HPEN ray_pen =
        CreatePen(PS_SOLID, 1, on ? RGB(255, 194, 79) : RGB(96, 101, 112));
    for (int ray = 0; ray < 8; ++ray) {
        double angle = ray * 3.14159265358979323846 / 4.0;
        int x1 = center_x + static_cast<int>(36 * std::cos(angle));
        int y1 = bulb_top + 29 + static_cast<int>(36 * std::sin(angle));
        int x2 = center_x + static_cast<int>(44 * std::cos(angle));
        int y2 = bulb_top + 29 + static_cast<int>(44 * std::sin(angle));
        HGDIOBJ old_pen = SelectObject(dc, ray_pen);
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
        SelectObject(dc, old_pen);
    }
    DeleteObject(ray_pen);

    HBRUSH bulb_brush =
        CreateSolidBrush(on ? RGB(248, 183, 67) : RGB(81, 86, 97));
    HPEN bulb_pen =
        CreatePen(PS_SOLID, 2, on ? RGB(255, 207, 115) : RGB(105, 111, 123));
    HGDIOBJ old_brush = SelectObject(dc, bulb_brush);
    HGDIOBJ old_pen = SelectObject(dc, bulb_pen);
    Ellipse(dc, center_x - 25, bulb_top, center_x + 25, bulb_top + 58);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(bulb_pen);
    DeleteObject(bulb_brush);

    FillRoundRect(dc, {center_x - 15, bulb_top + 52, center_x + 15,
                       bulb_top + 68},
                  4, RGB(111, 116, 126), RGB(151, 156, 166));
    FillRoundRect(dc, {center_x - 11, bulb_top + 68, center_x + 11,
                       bulb_top + 80},
                  4, RGB(91, 96, 106), RGB(137, 142, 153));

    HPEN wave_pen =
        CreatePen(PS_SOLID, 3, on ? RGB(87, 60, 17) : RGB(44, 48, 56));
    HGDIOBJ old_pen2 = SelectObject(dc, wave_pen);
    MoveToEx(dc, center_x - 14, bulb_top + 28, nullptr);
    LineTo(dc, center_x - 4, bulb_top + 20);
    LineTo(dc, center_x + 5, bulb_top + 33);
    LineTo(dc, center_x + 14, bulb_top + 25);
    SelectObject(dc, old_pen2);
    DeleteObject(wave_pen);
}

void View::DrawPowerButton(HDC dc, const RECT& rect, bool on) const {
    int center_x = (rect.left + rect.right) / 2;
    int center_y = (rect.top + rect.bottom) / 2;
    COLORREF color = on ? kAccent : kMuted;
    HPEN ring_pen = CreatePen(PS_SOLID, 4, color);
    HPEN symbol_pen = CreatePen(PS_SOLID, 3, color);
    HGDIOBJ old_pen = SelectObject(dc, ring_pen);
    Ellipse(dc, rect.left, rect.top, rect.right, rect.bottom);
    SelectObject(dc, old_pen);
    old_pen = SelectObject(dc, symbol_pen);
    Arc(dc, center_x - 11, center_y - 10, center_x + 11, center_y + 12,
        center_x - 8, center_y - 1, center_x + 8, center_y - 1);
    MoveToEx(dc, center_x, center_y - 15, nullptr);
    LineTo(dc, center_x, center_y - 2);
    SelectObject(dc, old_pen);
    DeleteObject(ring_pen);
    DeleteObject(symbol_pen);
}

void View::DrawSlider(HDC dc, const RECT& track, int percent) const {
    int knob_x =
        track.left + (track.right - track.left) * percent / 100;
    RECT active = track;
    FillRoundRect(dc, track, 7, RGB(49, 54, 64), CLR_INVALID);
    active.right = knob_x;
    if (active.right > active.left) {
        FillRoundRect(dc, active, 7, kAccent, CLR_INVALID);
    }
    HBRUSH knob_brush = CreateSolidBrush(RGB(249, 247, 243));
    HPEN knob_pen = CreatePen(PS_SOLID, 1, RGB(214, 210, 203));
    HGDIOBJ old_brush = SelectObject(dc, knob_brush);
    HGDIOBJ old_pen = SelectObject(dc, knob_pen);
    Ellipse(dc, knob_x - 9, track.top - 5, knob_x + 9, track.bottom + 5);
    SelectObject(dc, old_pen);
    SelectObject(dc, old_brush);
    DeleteObject(knob_pen);
    DeleteObject(knob_brush);
}

void View::DrawPresets(HDC dc, int index) const {
    for (int preset = 0; preset < kPresetCount; ++preset) {
        RECT rect = PresetRect(index, preset);
        std::wstring text = std::to_wstring(kPresetValues[preset]) + L"%";
        FillRoundRect(dc, rect, 4, RGB(37, 41, 50), kCardBorder);
        DrawText(dc, text, rect, FontId::Small, kText,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// ---------------------------------------------------------------------------
// 文本辅助
// ---------------------------------------------------------------------------

std::wstring View::Utf8ToWide(const std::string& text) const {
    if (text.empty()) {
        return {};
    }
    int length = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                     static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        std::wstring fallback;
        for (char c : text) {
            fallback.push_back(static_cast<wchar_t>(static_cast<unsigned char>(c)));
        }
        return fallback;
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), length);
    return result;
}

std::wstring View::TelemetryLine1() const {
    const DeviceStatus& status = model_.status;
    if (status.version < 0) {
        return L"状态：--";
    }
    std::wstring text = L"V" + std::to_wstring(status.version);
    text += L" 模式 " +
            (status.mode < 0 ? L"--" : std::to_wstring(status.mode));
    text += L" PWM " +
            (status.pwm < 0 ? L"--" : std::to_wstring(status.pwm));
    text += L" 控制 " +
            (status.control < 0 ? L"--" : std::to_wstring(status.control));
    text += L" 温度 ";
    text += status.temperature < 0
                ? L"--"
                : std::to_wstring(status.temperature) + L"℃";
    return text;
}

std::wstring View::TelemetryLine2() const {
    const DeviceStatus& status = model_.status;
    if (status.version < 0) {
        return L"";
    }
    std::wstring text = L"电压 " + FormatVoltage(status.voltage_raw);
    text += L" 电流 " + FormatCurrent(status.current_raw);
    text += L" 保护 " +
            (status.error_flags < 0 ? L"--"
                                    : std::to_wstring(status.error_flags));
    return text;
}

std::wstring View::ConnectButtonText() const {
    switch (model_.connection_state) {
    case ConnectionState::Connecting:
        return L"连接中...";
    case ConnectionState::Connected:
        return L"断开";
    case ConnectionState::Reconnecting:
        return L"正在重连...";
    case ConnectionState::Disconnected:
    case ConnectionState::Failed:
        break;
    }
    return L"连接";
}

bool View::ConnectButtonEnabled() const {
    return model_.connection_state != ConnectionState::Connecting &&
           model_.connection_state != ConnectionState::Reconnecting;
}

std::wstring View::FormatVoltage(int raw_value) const {
    if (raw_value < 0) {
        return L"--";
    }
    wchar_t buffer[16];
    std::swprintf(buffer, 16, L"%.1fV", raw_value / 10.0);
    return std::wstring(buffer);
}

std::wstring View::FormatCurrent(int raw_value) const {
    if (raw_value < 0) {
        return L"--";
    }
    wchar_t buffer[16];
    std::swprintf(buffer, 16, L"%.1fA", raw_value / 10.0);
    return std::wstring(buffer);
}

}  // namespace gui
}  // namespace lamp
