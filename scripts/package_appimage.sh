#!/bin/bash
set -e

# Configuration
APP_NAME="PixelBridge"
BUILD_DIR="build/release"
APP_DIR="AppDir"

# Tools (Assumed to be downloaded by Workflow/CI)
LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"

# Clean up and prepare AppDir
rm -rf $APP_DIR
mkdir -p $APP_DIR

# Ensure build is up to date and installed to AppDir
# Use absolute path for DESTDIR to avoid confusion
DESTDIR=$(pwd)/$APP_DIR cmake --build $BUILD_DIR --target install

# Run linuxdeploy
# export QMAKE=/usr/lib/qt6/bin/qmake # Adjust path if needed if not in PATH
export VERSION=1.0.0
# Tell the Qt plugin where to look for QML files to bundle dependencies
export QML_SOURCES_PATHS="qml"
# Force inclusion of wayland platform plugin for Wayland screen capture support
export EXTRA_QT_PLUGINS="wayland;wayland-egl"

$LINUXDEPLOY \
    --appdir $APP_DIR \
    --plugin qt \
    --output appimage \
    --desktop-file PixelBridge.desktop \
    --icon-file assets/icons/pixelbridge.svg \
    --executable $APP_DIR/usr/bin/PixelBridge

echo "AppImage generated successfully!"
