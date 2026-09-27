# 智能灯光控制器（LampController）

通过串口控制多路 LED 灯光设备的桌面程序，提供**图形界面**、**命令行工具**与 **C SDK 动态库**三种使用方式。图形界面适合日常开关灯、亮度调节和设备状态查看；命令行工具适合批量操作、脚本调用和快速调试；SDK 面向需要集成控制能力的其他软件。项目使用 **C++17** 编写，采用分层架构，通过 **CMake** 构建，支持 Windows 平台。

## 构建方法

需要已安装 CMake 与支持 C++17 的编译器（本仓库在 MSYS2 UCRT64 的 GCC 15 上验证）。在项目根目录执行：

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

运行单元测试：

```bash
ctest --test-dir build
```

若需要生成 Debug 版本，在配置时附加 `-DCMAKE_BUILD_TYPE=Debug`。制作 Windows 安装包执行：

```bash
cmake --build build --target package_nsis
```

生成 `build\LampControllerSetup-1.0.0.exe`。该目标需要先安装 NSIS 3（默认安装路径 `C:\Program Files (x86)\NSIS`）。构建过程会自动将 MinGW 运行库（`libgcc_s_seh-1.dll`、`libstdc++-6.dll`、`libwinpthread-1.dll`）复制到构建目录并随安装包分发，目标机器无需安装 MSYS2。

## 产物说明

| 产物 | 说明 |
|------|------|
| `lamp_gui.exe` | 图形界面程序（安装包安装后命名为"智能灯光控制器.exe"） |
| `lampctl.exe` | 命令行程序 |
| `libLampController.Sdk.dll` | C SDK 动态库 |
| `libLampController.Sdk.dll.a` | SDK 导入库，供 C/C++ 程序链接使用 |
| `LampSdkDemo.exe` | SDK 使用示例 |
| `lamp_tests.exe` | 单元测试程序（由 ctest 调用） |
| `LampControllerSetup-1.0.0.exe` | NSIS 安装包 |

## 目录结构

```
include/lamp   公共头文件，按核心、传输、平台、服务、配置、日志、SDK 分层组织
src            实现代码，gui 目录内按 controller、model、view 三层组织图形界面
tests          单元测试
docs           协议与架构文档
packaging      NSIS 安装脚本
```

日志文件默认写入运行目录下的 `logs` 文件夹。

## 图形界面使用

使用串口线连接灯光控制设备后，双击 `lamp_gui.exe`（或运行安装包安装后点击桌面快捷方式）。在"设备连接"区域选择实际串口，默认波特率 19200 一般不需要修改，点击"连接设备"。连接后可以分别控制四个通道：

- 点击电源按钮打开或关闭对应灯
- 拖动滑块调整亮度
- 点击快速设置按钮设定常用亮度
- 点击"应用设置"重新下发当前参数

右侧"通信记录"显示连接和操作结果，设备卡片显示设备版本、温度、电压、电流和保护状态。程序会自动保存端口、波特率、调光参数和通道亮度，下次启动自动恢复；串口断开或设备拔插后会自动重连。

## lampctl 命令行使用

双击 `lampctl.exe` 会显示帮助信息并等待回车后关闭。实际使用请打开 PowerShell 或 CMD，切换到程序所在目录后执行命令。常用命令：

```bash
# 查看串口
lampctl ports

# 查看设备状态
lampctl status --port COM9

# 切换稳定数字调光 95K
lampctl stable --port COM9

# 打开全部灯，亮度 60%
lampctl on --port COM9 --brightness 60

# 只打开灯 1，亮度 60%
lampctl on --port COM9 --light 1 --brightness 60

# 关闭全部灯
lampctl off --port COM9

# 分别设置 CH1 为 60%、CH2 为 30%
lampctl set --port COM9 --light1 60 --light2 30

# 使用设备原始值 0-255 设置亮度
lampctl brightness --port COM9 --light 1 --raw 153

# 发送原始指令
lampctl raw --port COM9 --command "S_MOD:0#" --read-ms 300
```

所有命令均可加 `--baud` 覆盖默认波特率，端口可用 `--port` 指定或留空自动查找，`--light` 支持 `1`、`2`、`3`、`4` 或 `all`。部分命令支持 `--json` 输出结构化结果。

## 设备协议

协议为文本命令，以 `#` 结尾：

- `SPA<数值>#`、`SPB<数值>#`、`SPC<数值>#`、`SPD<数值>#`：通道 1-4 输出与亮度，数值 0-255
- `S_CTRL:1#`：允许通道输出；`S_MOD:0#` 稳定数字调光；`S_MOD:1#` 频闪模式；`S_PWMP:0#` 47.5K；`S_PWMP:1#` 95K
- `S_ALL#`：执行以上参数
- 状态查询：`S_VER:#`、`S_MOD:#`、`S_PWMP:#`、`S_CTRL:#`、`S_TEMP:#`、`S_VOLT:#`、`S_CRV:#`、`S_ERRS:#`

完整协议见 [docs/protocol.md](docs/protocol.md)。

## C SDK 简要说明

C/C++ 程序包含 `include/lamp/sdk/lamp_sdk.h` 并链接 `libLampController.Sdk.dll.a` 导入库，然后调用 `lamp_sdk_create`、`lamp_sdk_connect`、`lamp_sdk_set_brightness_percent`、`lamp_sdk_set_brightness_percent_all`、`lamp_sdk_turn_on`、`lamp_sdk_turn_off`、`lamp_sdk_apply_settings`、`lamp_sdk_query_status`、`lamp_sdk_destroy` 等接口。完整接口、返回值和示例请查看头文件与 `LampSdkDemo` 源码。

## 注意事项

- 同一个串口不能被两个程序同时打开。
- 连接失败时请确认端口名称正确且没有被其他串口工具占用。
- 端口不存在时请检查 USB 串口驱动和设备连接。
- 关闭所有正在使用串口的程序后拔插 USB 串口设备，可释放被占用的端口。
- 通道 3 与通道 4 的协议命令 `SPC`、`SPD` 依据通道命名规律扩展，若设备固件不支持请以真实设备验证结果为准。