智能灯光控制器（LampController）

项目简介

本程序用于通过串口控制双路至四路 LED 灯光控制设备，提供图形界面、命令行工具、C SDK 动态库三种使用方式。图形界面适合日常开关灯、亮度调节和设备状态查看；命令行工具适合批量操作、脚本调用和快速调试；SDK 供其他软件集成控制能力。项目使用 C++17 编写，采用分层架构，通过 CMake 构建。

一、构建方法

项目使用 CMake 构建，需要已安装 CMake 与支持 C++17 的编译器（本仓库在 MSYS2 UCRT64 的 GCC 15 上验证）。在项目根目录执行以下命令创建构建目录并编译全部目标。

  cmake -S . -B build -G "MinGW Makefiles"
  cmake --build build -j

编译产物位于 build 目录下。运行单元测试执行 ctest --test-dir build。若需要生成 Debug 版本，在配置时附加 -DCMAKE_BUILD_TYPE=Debug。

二、产物说明

build/智能灯光控制器.exe 为图形界面程序；build/lampctl.exe 为命令行程序；build/LampController.Sdk.dll 与 build/libLampController.Sdk.dll.a 为 C SDK 动态库及其导入库，供其他 C/C++ 程序链接调用。build/LampSdkDemo.exe 为 SDK 使用示例。

三、目录结构

include/lamp 存放公共头文件，按核心、传输、平台、服务、配置、日志、SDK 分层组织。src 存放实现，gui 目录内又按 controller、model、view 三层组织图形界面。tests 存放单元测试。docs 存放协议与架构文档。日志文件默认写入运行目录下的 logs 文件夹。

四、图形界面使用

使用串口线连接灯光控制设备后，双击智能灯光控制器.exe。在设备连接区域选择实际串口，默认波特率 19200 一般不需要修改，点击连接设备。连接后可以分别控制四个通道，点击电源按钮打开或关闭对应灯，拖动滑块调整亮度，点击快速设置按钮设定常用亮度，点击应用设置重新下发当前参数。右侧通信记录显示连接和操作结果，底部状态栏显示设备版本、温度、电压、电流和保护状态。程序会自动保存端口、波特率、调光参数和通道亮度，下次启动自动恢复；串口断开或设备拔插后会自动重连。

五、lampctl 命令行使用

双击 lampctl.exe 会显示帮助信息并等待回车后关闭。实际使用请打开 PowerShell 或 CMD，切换到程序所在目录后执行命令。查看串口执行 lampctl ports；查看设备状态执行 lampctl status --port COM9；切换稳定数字调光 95K 执行 lampctl stable --port COM9；打开两盏灯亮度 60% 执行 lampctl on --port COM9 --brightness 60；只打开灯 1 亮度 60% 执行 lampctl on --port COM9 --light 1 --brightness 60；关闭两盏灯执行 lampctl off --port COM9；分别设置 CH1 为 60% CH2 为 30% 执行 lampctl set --port COM9 --light1 60 --light2 30；使用原始值设置亮度执行 lampctl brightness --port COM9 --light 1 --raw 153；发送原始指令执行 lampctl raw --port COM9 --command "S_MOD:0#" --read-ms 300。所有命令均可加 --baud 覆盖默认波特率，端口可用 --port 指定或留空自动查找，--light 支持 1、2、3、4 或 all。部分命令支持 --json 输出结构化结果。

六、设备协议

协议为文本命令，以 # 结尾。SPA 数值 # 为通道 1 输出亮度，SPB 为通道 2，SPC 为通道 3，SPD 为通道 4，数值为 0 至 255。S_CTRL:1# 允许通道输出，S_MOD:0# 稳定数字调光，S_MOD:1# 频闪模式，S_PWMP:0# 47.5K，S_PWMP:1# 95K，S_ALL# 执行以上参数。状态查询命令包括 S_VER:#、S_MOD:#、S_PWMP:#、S_CTRL:#、S_TEMP:#、S_VOLT:#、S_CRV:#、S_ERRS:#。完整协议见 docs 目录下的协议文档。

七、C SDK 简要说明

C/C++ 程序包含 include/lamp/sdk/lamp_sdk.h 并链接 LampController.Sdk 导入库，然后调用 lamp_sdk_create、lamp_sdk_connect、lamp_sdk_set_brightness_percent、lamp_sdk_set_brightness_percent_all、lamp_sdk_turn_on、lamp_sdk_turn_off、lamp_sdk_apply_settings、lamp_sdk_query_status、lamp_sdk_destroy 等接口。完整接口、返回值和示例请查看头文件与 LampSdkDemo 源码。

八、注意事项

同一个串口不能被两个程序同时打开。连接失败时请确认端口名称正确并且没有被其他串口工具占用。如果端口不存在，请检查 USB 串口驱动和设备连接。关闭所有正在使用串口的程序后拔插 USB 串口设备，可以释放被占用的端口。通道 3 与通道 4 的协议命令 SPC 与 SPD 依据通道命名规律扩展，若设备固件不支持请以真实设备验证结果为准。
