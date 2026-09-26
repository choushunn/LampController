cmake_minimum_required(VERSION 3.16)
project(LampController VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

set(LAMP_CORE_SOURCES
  src/core/protocol.cpp
  src/core/units.cpp
  src/transport/windows_serial.cpp
  src/transport/mock_serial.cpp
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

add_executable(lamp_gui
  src/gui/main.cpp
  src/gui/app.cpp
  src/gui/controller.cpp
  src/gui/view.cpp
)
target_include_directories(lamp_gui PUBLIC include src/gui)
target_link_libraries(lamp_gui PRIVATE lamp_core user32 gdi32)
set_target_properties(lamp_gui PROPERTIES
  WIN32_EXECUTABLE TRUE
  OUTPUT_NAME "智能灯光控制器"
)

add_executable(LampSdkDemo src/demo/main.cpp)
target_include_directories(LampSdkDemo PUBLIC include)
target_link_libraries(LampSdkDemo PRIVATE LampController.Sdk)
set_target_properties(LampSdkDemo PROPERTIES OUTPUT_NAME LampSdkDemo)

enable_testing()
add_executable(lamp_tests
  tests/test_main.cpp
  tests/test_harness.h
  tests/protocol_test.cpp
  tests/units_test.cpp
  tests/port_list_test.cpp
  tests/config_test.cpp
  tests/state_machine_test.cpp
)
target_include_directories(lamp_tests PUBLIC include tests)
target_link_libraries(lamp_tests PRIVATE lamp_core)
add_test(NAME lamp_tests COMMAND lamp_tests)
