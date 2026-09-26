#include "app.h"

#include "controller.h"
#include "model.h"
#include "view.h"

#include "lamp/config/app_config.h"

namespace lamp {
namespace gui {

namespace {

constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                               WS_MINIMIZEBOX;
constexpr int kWindowWidth = 1000;
constexpr int kWindowHeight = 780;
const wchar_t* kFontFamily = L"Microsoft YaHei UI";

}  // namespace

App::App() = default;
App::~App() = default;

int App::Run(HINSTANCE instance, int show_command) {
    RegisterWindowClass(instance);

    RECT rectangle = {0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRectEx(&rectangle, kWindowStyle, FALSE, WS_EX_APPWINDOW);

    hwnd_ = CreateWindowExW(
        WS_EX_APPWINDOW, kWindowClass, kWindowTitle, kWindowStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rectangle.right - rectangle.left, rectangle.bottom - rectangle.top,
        nullptr, nullptr, instance, this);
    if (hwnd_ == nullptr) {
        return 1;
    }

    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void App::RegisterWindowClass(HINSTANCE instance) {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_DBLCLKS;
    window_class.lpfnWndProc = View::WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground =
        static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    window_class.lpszClassName = kWindowClass;
    window_class.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    window_class.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&window_class);
}

void App::OnCreate(HWND hwnd) {
    hwnd_ = hwnd;
    CreateFonts();

    AppConfigData config = LoadAppConfig(DefaultConfigPath());
    model_ = std::make_unique<UiModel>(config);
    controller_ = std::make_unique<Controller>(*model_, hwnd, config);
    view_ = std::make_unique<View>(*this, *controller_, *model_, hwnd);

    // 启动时自动刷新端口列表，并按配置中的端口名默认选中。
    controller_->OnPortRefresh();
    controller_->AppendUILog(
        "程序已启动。连接设备后将自动切换到稳定数字调光 / 95K。");

    SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(view_.get()));
}

void App::OnDestroy() {
    DestroyFonts();
    view_.reset();
    controller_.reset();
    model_.reset();
    hwnd_ = nullptr;
}

void App::CreateFonts() {
    fonts_[static_cast<size_t>(FontId::Title)] = CreateFontW(
        -31, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::Subtitle)] = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::CardTitle)] = CreateFontW(
        -22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::Label)] = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::Value)] = CreateFontW(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::Button)] = CreateFontW(
        -15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::Percent)] = CreateFontW(
        -42, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
    fonts_[static_cast<size_t>(FontId::Log)] = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        FIXED_PITCH, L"Consolas");
    fonts_[static_cast<size_t>(FontId::Small)] = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, kFontFamily);
}

void App::DestroyFonts() {
    for (auto& font : fonts_) {
        if (font != nullptr) {
            DeleteObject(font);
            font = nullptr;
        }
    }
}

}  // namespace gui
}  // namespace lamp
