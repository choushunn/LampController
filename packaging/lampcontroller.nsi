; 智能灯光控制器（LampController）NSIS 安装脚本
; 通过 CMake 目标 package_nsis 调用：
;   makensis /DBUILD_DIR=<build> /DSRC_DIR=<source> packaging/lampcontroller.nsi
; 产物：build\LampControllerSetup-1.0.0.exe

Unicode true
XPStyle on
RequestExecutionLevel admin

; 安装包与卸载程序使用与应用相同的图标
Icon "${SRC_DIR}\resources\app.ico"
UninstallIcon "${SRC_DIR}\resources\app.ico"

!include "MUI2.nsh"

Name "智能灯光控制器"
OutFile "${BUILD_DIR}\LampControllerSetup-1.0.0.exe"
InstallDir "$PROGRAMFILES64\LampController"
InstallDirRegKey HKLM "Software\LampController" "InstallDir"

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "智能灯光控制器"
VIAddVersionKey "FileDescription" "智能灯光控制器安装程序"
VIAddVersionKey "ProductVersion" "1.0.0"
VIAddVersionKey "FileVersion" "1.0.0"

!define MUI_WELCOMEPAGE_TITLE "欢迎安装 智能灯光控制器"
!define MUI_FINISHPAGE_RUN "$INSTDIR\智能灯光控制器.exe"
!define MUI_FINISHPAGE_RUN_TEXT "立即运行智能灯光控制器"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

Section "主程序" SecMain
    SetOutPath "$INSTDIR"

    File /oname=智能灯光控制器.exe "${BUILD_DIR}\lamp_gui.exe"
    File "${BUILD_DIR}\lampctl.exe"
    File "${BUILD_DIR}\LampSdkDemo.exe"
    File "${BUILD_DIR}\libLampController.Sdk.dll"
    File "${BUILD_DIR}\libLampController.Sdk.dll.a"
    File "${BUILD_DIR}\libgcc_s_seh-1.dll"
    File "${BUILD_DIR}\libstdc++-6.dll"
    File "${BUILD_DIR}\libwinpthread-1.dll"
    File "${SRC_DIR}\README.md"
    ; File 指令不会自动创建目标子目录，需先行切换输出目录
    SetOutPath "$INSTDIR\docs"
    File "${SRC_DIR}\docs\protocol.md"
    File "${SRC_DIR}\docs\使用说明.md"
    SetOutPath "$INSTDIR"

    ; 开始菜单快捷方式
    CreateDirectory "$SMPROGRAMS\智能灯光控制器"
    CreateShortCut "$SMPROGRAMS\智能灯光控制器\智能灯光控制器.lnk" "$INSTDIR\智能灯光控制器.exe"
    CreateShortCut "$SMPROGRAMS\智能灯光控制器\命令行工具.lnk" "$INSTDIR\lampctl.exe"
    CreateShortCut "$SMPROGRAMS\智能灯光控制器\使用说明.lnk" "$WINDIR\notepad.exe" "$INSTDIR\docs\使用说明.md"
    CreateShortCut "$SMPROGRAMS\智能灯光控制器\卸载.lnk" "$INSTDIR\Uninstall.exe"

    ; 桌面快捷方式
    CreateShortCut "$DESKTOP\智能灯光控制器.lnk" "$INSTDIR\智能灯光控制器.exe"

    ; 卸载器与注册表卸载信息
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "DisplayName" "智能灯光控制器"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "DisplayVersion" "1.0.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "Publisher" "LampController"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "DisplayIcon" "$INSTDIR\智能灯光控制器.exe"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController" "NoRepair" 1
    WriteRegStr HKLM "Software\LampController" "InstallDir" "$INSTDIR"
SectionEnd

Section "Uninstall"
    Delete "$DESKTOP\智能灯光控制器.lnk"
    RMDir /r "$SMPROGRAMS\智能灯光控制器"
    RMDir /r "$INSTDIR\logs"
    Delete "$INSTDIR\智能灯光控制器.exe"
    Delete "$INSTDIR\lampctl.exe"
    Delete "$INSTDIR\LampSdkDemo.exe"
    Delete "$INSTDIR\libLampController.Sdk.dll"
    Delete "$INSTDIR\libLampController.Sdk.dll.a"
    Delete "$INSTDIR\libgcc_s_seh-1.dll"
    Delete "$INSTDIR\libstdc++-6.dll"
    Delete "$INSTDIR\libwinpthread-1.dll"
    Delete "$INSTDIR\docs\使用说明.md"
    Delete "$INSTDIR\README.md"
    Delete "$INSTDIR\docs\protocol.md"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR\docs"
    RMDir "$INSTDIR"

    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\LampController"
    DeleteRegKey HKLM "Software\LampController"
SectionEnd
