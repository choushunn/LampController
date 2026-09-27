cmake_minimum_required(VERSION 3.16)
project(LampController VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# 单元测试仅在 BUILD_TESTING=ON 时构建；mock 串口实现只服务于测试，
# 独立成 lamp_test_utils 库，不进入正式库 lamp_core。
option(BUILD_TESTING "Build and run unit tests" ON)

# 可选的 sanitizer 构建（-fsanitize=address,undefined），默认关闭；
# 仅推荐用于调试定位内存与时序问题，Release 打包请保持关闭。
option(LAMP_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(LAMP_ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

set(LAMP_CORE_SOURCES
  src/core/model.cpp
  src/core/protocol.cpp
  src/core/units.cpp
  src/transport/windows_serial.cpp
  src/platform/windows_port_discovery.cpp
  src/service/connection_state.cpp
  src/service/lamp_device.cpp
  src/config/app_config.cpp
  src/logging/logger.cpp
)

add_library(lamp_core STATIC ${LAMP_CORE_SOURCES})
target_include_directories(lamp_core PUBLIC include)
target_link_libraries(lamp_core PRIVATE advapi32)

add_library(LampController.Sdk SHARED src/sdk/lamp_sdk.cpp)
target_include_directories(LampController.Sdk PUBLIC include)
target_compile_definitions(LampController.Sdk PRIVATE LAMP_SDK_EXPORTS)
target_link_libraries(LampController.Sdk PRIVATE lamp_core)
set_target_properties(LampController.Sdk PROPERTIES OUTPUT_NAME LampController.Sdk)

add_executable(lampctl src/cli/main.cpp)
target_include_directories(lampctl PUBLIC include)
target_link_libraries(lampctl PRIVATE lamp_core)
set_target_properties(lampctl PROPERTIES OUTPUT_NAME lampctl)

# 构建产物统一使用 ASCII 文件名。mingw32-make 无法可靠处理中文文件名，
# 因此图形界面程序构建为 lamp_gui.exe，安装包制作时再由 NSIS 改名为
# 智能灯光控制器.exe 安装。
add_executable(lamp_gui
  src/gui/main.cpp
  src/gui/app.cpp
  src/gui/controller.cpp
  src/gui/view.cpp
  resources/app.rc
)
target_include_directories(lamp_gui PUBLIC include src/gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lamp_gui PRIVATE lamp_core user32 gdi32)
set_target_properties(lamp_gui PROPERTIES
  WIN32_EXECUTABLE TRUE
  OUTPUT_NAME lamp_gui
)

add_executable(LampSdkDemo src/demo/main.cpp)
target_include_directories(LampSdkDemo PUBLIC include)
target_link_libraries(LampSdkDemo PRIVATE LampController.Sdk)
set_target_properties(LampSdkDemo PROPERTIES OUTPUT_NAME LampSdkDemo)

if(BUILD_TESTING)
  enable_testing()

  # mock 串口仅测试使用，独立成库，避免正式 lamp_core 混入测试代码。
  add_library(lamp_test_utils STATIC src/transport/mock_serial.cpp)
  target_include_directories(lamp_test_utils PUBLIC include)

  add_executable(lamp_tests
    tests/test_main.cpp
    tests/protocol_test.cpp
    tests/units_test.cpp
    tests/port_list_test.cpp
    tests/config_test.cpp
    tests/state_machine_test.cpp
    tests/device_test.cpp
  )
  target_include_directories(lamp_tests PUBLIC include tests)
  target_link_libraries(lamp_tests PRIVATE lamp_core lamp_test_utils)
  add_test(NAME lamp_tests COMMAND lamp_tests)

  # SDK C ABI 中不依赖真实设备的部分（转换、建帧、句柄生命周期等）做自动化测试。
  add_executable(lamp_sdk_tests
    tests/test_main.cpp
    tests/sdk_test.cpp
  )
  target_include_directories(lamp_sdk_tests PUBLIC include tests)
  target_link_libraries(lamp_sdk_tests PRIVATE LampController.Sdk)
  add_test(NAME lamp_sdk_tests COMMAND lamp_sdk_tests)
endif()

# 将 MinGW 运行时 DLL 复制到构建目录，供 NSIS 安装包随附分发，
# 保证目标机器无需安装 MSYS2 即可运行所有程序。
set(MINGW_RUNTIME_DLLS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(COPY_RUNTIME_DLLS "")
foreach(dll IN LISTS MINGW_RUNTIME_DLLS)
  list(APPEND COPY_RUNTIME_DLLS COMMAND "${CMAKE_COMMAND}" -E copy_if_different
       "${MINGW_BIN_DIR}/${dll}" "${CMAKE_BINARY_DIR}/${dll}")
endforeach()
add_custom_target(copy_runtime_dlls ALL
  COMMAND ${COPY_RUNTIME_DLLS}
  COMMENT "Copying MinGW runtime DLLs for packaging"
  VERBATIM
)
add_dependencies(copy_runtime_dlls lamp_gui lampctl LampSdkDemo LampController.Sdk)

# NSIS installer packaging. Run `cmake --build build --target package_nsis`
# after a full build; requires NSIS (makensis) to be installed.
find_program(NSIS_EXECUTABLE makensis
  PATHS "C:/Program Files (x86)/NSIS"
)
if(NSIS_EXECUTABLE)
  add_custom_target(package_nsis
    COMMAND "${NSIS_EXECUTABLE}" /V4
      "/DBUILD_DIR=${CMAKE_BINARY_DIR}" "/DSRC_DIR=${CMAKE_SOURCE_DIR}"
      "${CMAKE_SOURCE_DIR}/packaging/lampcontroller.nsi"
    DEPENDS lamp_gui lampctl LampSdkDemo LampController.Sdk copy_runtime_dlls
    COMMENT "Building NSIS installer LampControllerSetup-1.0.0.exe"
    VERBATIM
  )
endif()
