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
        mainwindow.cpp
        mainwindow.h
        chartview.cpp
        chartview.h
        recorder.cpp
        recorder.h
        netserver.cpp
        netserver.h
        csvio.cpp
        csvio.h
        protocol.cpp
        protocol.h
        model.cpp
        model.h
        decimate.cpp
        decimate.h
        config.h
        theme.h
)

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
