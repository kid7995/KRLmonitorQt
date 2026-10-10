cmake_minimum_required(VERSION 3.16)

project(KRLmonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源码为 UTF-8（含中文注释/字面量），MSVC 默认按 GBK 代码页解析会报错
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(PROJECT_SOURCES
        main.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/chartview.cpp
        src/chartview.h
        src/recorder.cpp
        src/recorder.h
        src/netserver.cpp
        src/netserver.h
        src/csvio.cpp
        src/csvio.h
        src/protocol.cpp
        src/protocol.h
        src/station.cpp
        src/station.h
        src/model.cpp
        src/model.h
        src/decimate.cpp
        src/decimate.h
        src/config.h
        src/theme.h
)

# Windows 可执行文件图标。.rc 与图标先复制到构建目录，避免资源编译器
# 因不同生成器的工作目录差异而找不到 fig/logo.ico。
if(WIN32)
    set(KRLMONITOR_ICON "${CMAKE_CURRENT_SOURCE_DIR}/fig/logo.ico")
    if(NOT EXISTS "${KRLMONITOR_ICON}")
        message(FATAL_ERROR "应用图标不存在：${KRLMONITOR_ICON}")
    endif()
    configure_file("${KRLMONITOR_ICON}" "${CMAKE_CURRENT_BINARY_DIR}/logo.ico" COPYONLY)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc"
        "${CMAKE_CURRENT_BINARY_DIR}/KRLmonitor.rc"
        COPYONLY
    )
    list(APPEND PROJECT_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/KRLmonitor.rc")
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(KRLmonitor
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(KRLmonitor
        ${PROJECT_SOURCES}
    )
endif()

target_include_directories(KRLmonitor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(KRLmonitor PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

set_target_properties(KRLmonitor PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(KRLmonitor)
endif()

# ── 测试 ────────────────────────────────────────────────────────
include(CTest)
if(BUILD_TESTING)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()