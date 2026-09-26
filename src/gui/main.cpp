#include "app.h"

#include <windows.h>

// 注意：CMake 的 lamp_gui 目标以 -mwindows 链接且未启用 -municode，
// 因此这里必须使用 ANSI 入口 WinMain（内部全部调用 W 后缀 API）。
int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous_instance,
                   LPSTR command_line, int show_command) {
    (void)previous_instance;
    (void)command_line;
    SetProcessDPIAware();

    lamp::gui::App app;
    return app.Run(instance, show_command);
}
