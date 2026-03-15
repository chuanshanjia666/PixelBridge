cmake_minimum_required(VERSION 3.14)

# Read version from VERSION file
file(READ VERSION PIXELBRIDGE_VERSION_RAW)
string(STRIP "${PIXELBRIDGE_VERSION_RAW}" PIXELBRIDGE_VERSION)

project(PixelBridge VERSION ${PIXELBRIDGE_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically initialize git submodules (e.g. thirdparty/live555) if the
# source tree is a git repo and they haven't been checked out yet.
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/live555/liveMedia/include/liveMedia.hh")
        message(STATUS "Initializing git submodules...")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR
                "git submodule update --init failed (exit ${GIT_SUBMOD_RESULT}). "
                "Please run 'git submodule update --init --recursive' manually.")
        endif()
    endif()
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/live555/liveMedia/include/liveMedia.hh")
    message(FATAL_ERROR
        "thirdparty/live555 submodule is not initialized. "
        "Please run: git submodule update --init --recursive")
endif()

# Suppress AUTOMOC warnings for .hh files (common in live555)
cmake_policy(SET CMP0100 NEW)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)
if(UNIX AND NOT APPLE)
    find_package(Qt6 COMPONENTS WaylandClient)
endif()
find_package(Threads REQUIRED)
find_package(FFmpeg COMPONENTS AVFORMAT AVCODEC AVUTIL AVDEVICE SWSCALE REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)

add_subdirectory(thirdparty)

include_directories(include)
include_directories(${CMAKE_BINARY_DIR}/include)

# Configure version header
configure_file(${CMAKE_SOURCE_DIR}/include/core/Version.h.in ${CMAKE_BINARY_DIR}/include/core/Version.h)
set(SOURCES
    src/main.cpp
    src/core/Bridge.cpp
    include/core/Bridge.h
    src/core/Logger.cpp
    include/core/Logger.h
    src/filters/Demuxer.cpp
    src/filters/VideoDecoder.cpp
    src/filters/VideoEncoder.cpp
    src/filters/Muxer.cpp
    src/filters/RtspServerFilter.cpp
    src/filters/QmlVideoSinkFilter.cpp
    include/filters/QmlVideoSinkFilter.h
    resources.qrc
)

# QScreenCapture was introduced in Qt 6.5 and has no Android backend.
# Only compile ScreenCapture when Qt >= 6.5 AND not targeting Android so that
# MOC never sees the Q_OBJECT class on unsupported configurations.
if(NOT ANDROID AND Qt6_VERSION VERSION_GREATER_EQUAL "6.5.0")
    list(APPEND SOURCES
        src/filters/ScreenCapture.cpp
        include/filters/ScreenCapture.h
    )
    message(STATUS "QScreenCapture enabled (Qt ${Qt6_VERSION})")
else()
    message(STATUS "QScreenCapture disabled (Qt ${Qt6_VERSION}, Android=${ANDROID}): requires Qt >= 6.5 on non-Android")
endif()

if(WIN32)
    add_executable(PixelBridge WIN32 ${SOURCES})
else()
    add_executable(PixelBridge ${SOURCES})
endif()

if(APPLE)
    set_target_properties(PixelBridge PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "PixelBridge"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.pixelbridge.app"
        MACOSX_BUNDLE_INFO_STRING "Efficient Screen Capture and RTSP Server"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

target_link_libraries(PixelBridge
    PRIVATE
    FFmpeg::avformat
    FFmpeg::avcodec
    FFmpeg::avutil
    FFmpeg::avdevice
    FFmpeg::swscale
    spdlog::spdlog
    liveMedia
    groupsock
    BasicUsageEnvironment
    UsageEnvironment
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Multimedia
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(Qt6WaylandClient_FOUND)
    target_link_libraries(PixelBridge PRIVATE Qt6::WaylandClient)
endif()

if(WIN32)
    target_link_libraries(PixelBridge PRIVATE psapi)
endif()

if(ANDROID)
    # Android-specific target properties for Qt APK packaging
    set_target_properties(PixelBridge PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION 26
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_VERSION_CODE 1
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    )
    qt_android_generate_deployment_settings(PixelBridge)
    qt_android_add_apk_target(PixelBridge)
endif()

# --- Installation ---
include(GNUInstallDirs)

install(TARGETS PixelBridge
    BUNDLE DESTINATION .
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
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Efficient Screen Capture and RTSP Server")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/chuanshanjia/PixelBridge")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "PixelBridge")
    set(CPACK_SYSTEM_NAME "macOS")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM")
    
    # Debian specific
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Chuanshanjia")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    
    # RPM specific
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_AUTOREQ ON)
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
endif()

include(CPack)
