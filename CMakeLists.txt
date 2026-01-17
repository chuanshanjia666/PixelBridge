cmake_minimum_required(VERSION 3.10)
project(PixelBridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(AVDEVICE REQUIRED libavdevice)
pkg_check_modules(SWSCALE REQUIRED libswscale)
pkg_check_modules(SPDLOG REQUIRED spdlog)

include_directories(include)
include_directories(${AVFORMAT_INCLUDE_DIRS})
include_directories(${AVCODEC_INCLUDE_DIRS})
include_directories(${AVUTIL_INCLUDE_DIRS})
include_directories(${AVDEVICE_INCLUDE_DIRS})
include_directories(${SWSCALE_INCLUDE_DIRS})
include_directories(${SPDLOG_INCLUDE_DIRS})

set(SOURCES
    src/main.cpp
    src/filters/Demuxer.cpp
    src/filters/VideoDecoder.cpp
    src/filters/ScreenCapture.cpp
    src/filters/VideoEncoder.cpp
    src/filters/Muxer.cpp
)

add_executable(PixelBridge ${SOURCES})

target_link_libraries(PixelBridge
    ${AVFORMAT_LIBRARIES}
    ${AVCODEC_LIBRARIES}
    ${AVUTIL_LIBRARIES}
    ${AVDEVICE_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${SPDLOG_LIBRARIES}
    pthread
)
