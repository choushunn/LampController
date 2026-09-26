#pragma once

#include <memory>

#include <windows.h>

namespace lamp {
namespace gui {

class Controller;
class UiModel;
class View;

// 字体资源标识（由 App 统一创建/销毁）。
enum class FontId {
    Title,
    Subtitle,
    CardTitle,
    Label,
    Value,
    Button,
    Percent,
    Log,
    Small,
    Count
};

// 应用外壳：注册窗口类、创建主窗口、组装 MVC 三层并持有字体资源。
class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // 注册窗口类、创建主窗口并进入消息循环，WM_QUIT 后返回退出码。
    int Run(HINSTANCE instance, int show_command);

    HWND Hwnd() const { return hwnd_; }
    View* GetView() const { return view_.get(); }
    HFONT Font(FontId id) const {
        return fonts_[static_cast<size_t>(id)];
    }

    // 由 View::WindowProc 在 WM_CREATE / WM_DESTROY 时调用。
    void OnCreate(HWND hwnd);
    void OnDestroy();

    static constexpr const wchar_t* kWindowClass = L"LampControllerModernWindow";
    static constexpr const wchar_t* kWindowTitle = L"智能灯光控制器";

private:
    void RegisterWindowClass(HINSTANCE instance);
    void CreateFonts();
    void DestroyFonts();

    HWND hwnd_ = nullptr;
    std::unique_ptr<UiModel> model_;
    std::unique_ptr<Controller> controller_;
    std::unique_ptr<View> view_;
    HFONT fonts_[static_cast<size_t>(FontId::Count)]{};
};

}  // namespace gui
}  // namespace lamp
