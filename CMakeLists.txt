cmake_minimum_required(VERSION 3.16)
project(SysMonitorCPP VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3 -pthread)
endif()

include_directories(include)

add_executable(sys_monitor
    src/main.cpp
    src/system_monitor.cpp
    src/process_parser.cpp
    src/tui_renderer.cpp
)

if(WIN32)
    target_link_libraries(sys_monitor pdh psapi iphlpapi ws2_32)
else()
    target_link_libraries(sys_monitor pthread)
endif()

enable_testing()
add_executable(test_monitor
    tests/test_metrics.cpp
    src/system_monitor.cpp
    src/process_parser.cpp
)
if(WIN32)
    target_link_libraries(test_monitor pdh psapi iphlpapi ws2_32)
else()
    target_link_libraries(test_monitor pthread)
endif()
add_test(NAME MetricsUnitTest COMMAND test_monitor)
