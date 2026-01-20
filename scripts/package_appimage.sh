#!/bin/bash
set -e

# Configuration
APP_NAME="PixelBridge"
BUILD_DIR="build/release"
APP_DIR="AppDir"

# Tools (Assumed to be downloaded by Workflow/CI)
LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"

# Ensure build is up to date and installed to AppDir
cmake --build $BUILD_DIR --target install DESTDIR=$APP_DIR

# Run linuxdeploy
# export QMAKE=/usr/lib/qt6/bin/qmake # Adjust path if needed if not in PATH
export VERSION=1.0.0

$LINUXDEPLOY \
    --appdir $APP_DIR \
    --plugin qt \
    --output appimage \
    --desktop-file PixelBridge.desktop \
    --icon-file assets/icons/pixelbridge.svg \
    --executable $APP_DIR/usr/local/bin/PixelBridge

echo "AppImage generated successfully!"
