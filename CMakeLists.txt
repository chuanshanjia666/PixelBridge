cmake_minimum_required(VERSION 3.10)
project(PixelBridge)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)
find_package(Threads REQUIRED)

# Use ECM for better FindFFmpeg support
find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(FFmpeg COMPONENTS AVFORMAT AVCODEC AVUTIL AVDEVICE SWSCALE REQUIRED)

# Find FFmpeg
find_package(OpenSSL REQUIRED)

find_package(spdlog REQUIRED)

add_subdirectory(thirdparty)

include_directories(include)
include_directories(${FFMPEG_INCLUDE_DIRS})
include_directories(${LIVE555_INCLUDE_DIRS})

set(SOURCES
    src/main.cpp
    src/core/Bridge.cpp
    include/core/Bridge.h
    src/core/Logger.cpp
    include/core/Logger.h
    src/filters/Demuxer.cpp
    src/filters/VideoDecoder.cpp
    src/filters/ScreenCapture.cpp
    include/filters/ScreenCapture.h
    src/filters/VideoEncoder.cpp
    src/filters/Muxer.cpp
    src/filters/RtspServerFilter.cpp
    src/filters/QmlVideoSinkFilter.cpp
    include/filters/QmlVideoSinkFilter.h
    resources.qrc
)

add_executable(PixelBridge ${SOURCES})

target_link_libraries(PixelBridge
    ${FFMPEG_LIBRARIES}
    spdlog::spdlog
    ${LIVE555_LIBRARIES}
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Threads::Threads
)

if(WIN32)
    target_link_libraries(PixelBridge psapi)
endif()

# --- Installation ---
include(GNUInstallDirs)

install(TARGETS PixelBridge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES assets/icons/pixelbridge.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
        RENAME pixelbridge.svg
    )
    install(FILES PixelBridge.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
endif()

# --- CPack Configuration ---
set(CPACK_PACKAGE_NAME "pixelbridge")
set(CPACK_PACKAGE_VENDOR "PixelBridge")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Efficient Screen Capture and RTSP Server")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/chuanshanjia/PixelBridge")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Chuanshanjia")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)

# add_executable(bench_qt_capture tests/bench_qt_capture.cpp)
# target_link_libraries(bench_qt_capture 
#     Qt6::Core 
#     Qt6::Gui 
#     Qt6::Multimedia
# )
