cmake_minimum_required(VERSION 3.10)
project(PixelBridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(AVDEVICE REQUIRED libavdevice)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(SPDLOG REQUIRED spdlog)

add_subdirectory(thirdparty)

include_directories(include)
include_directories(${AVFORMAT_INCLUDE_DIRS})
include_directories(${AVCODEC_INCLUDE_DIRS})
include_directories(${AVUTIL_INCLUDE_DIRS})
include_directories(${AVDEVICE_INCLUDE_DIRS})
include_directories(${SWSCALE_INCLUDE_DIRS})
include_directories(${SPDLOG_INCLUDE_DIRS})
include_directories(${LIVE555_INCLUDE_DIRS})

set(SOURCES
    src/main.cpp
    src/core/Bridge.cpp
    include/core/Bridge.h
    src/filters/Demuxer.cpp
    src/filters/VideoDecoder.cpp
    src/filters/ScreenCapture.cpp
    src/filters/VideoEncoder.cpp
    src/filters/Muxer.cpp
    src/filters/RtspServerFilter.cpp
    src/filters/QmlVideoSinkFilter.cpp
    include/filters/QmlVideoSinkFilter.h
    resources.qrc
)

add_executable(PixelBridge ${SOURCES})

target_link_libraries(PixelBridge
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${AVDEVICE_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SPDLOG_LIBRARIES}
    ${LIVE555_LIBRARIES}
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    pthread
)
